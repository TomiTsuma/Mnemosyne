"""LLM inference / generation (PRD §20). Best-effort transformers backend."""
from __future__ import annotations

import os
from typing import Any, Dict


def generate(spec: Dict[str, Any]) -> Dict[str, Any]:
    prompt = spec.get("prompt") or ""
    if not prompt:
        return {"ok": False, "error": "GENERATE requires a non-empty PROMPT"}

    model_name = os.environ.get("MNEMO_LLM_MODEL", "distilgpt2")
    try:
        from transformers import pipeline  # noqa: WPS433
    except ImportError:
        return {
            "ok": False,
            "error": ("LLM backend not installed. Install 'transformers' and 'torch' "
                      "to enable GENERATE (pip install transformers torch)."),
        }

    try:
        gen = pipeline("text-generation", model=model_name)
        out = gen(prompt, max_new_tokens=128, num_return_sequences=1)
        text = out[0].get("generated_text", "") if out else ""
        return {"ok": True, "generated_text": text}
    except Exception as exc:  # noqa: BLE001
        return {"ok": False, "error": f"LLM generation failed: {exc}"}
