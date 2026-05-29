// src/Parsers/grammar.y — ANTLR-style grammar definition for Mnemosyne SQL
// Mnemosyne: A column-oriented analytical DBMS
//
// This is a stub grammar — the actual parser uses recursive descent (parser_query.h).
// This file documents the grammar for reference and potential future ANTLR generation.

/*
grammar MnemosyneSQL;

// ── Top level ──
query:    query_stmt SEMICOLON* EOF ;

query_stmt:
      select_stmt
    | insert_stmt
    | create_table_stmt
    | drop_table_stmt
    | alter_table_stmt
    | explain_stmt
    ;

// ── SELECT ──
select_stmt:
      WITH cte_stmt (COMMA cte_stmt)*
      SELECT select_list
      (FROM from_clause)?
      (WHERE where_expr)?
      (GROUP BY group_by_list)?
      (HAVING having_expr)?
      (ORDER BY order_by_list)?
      (LIMIT limit_clause)?
      (UNION (ALL | DISTINCT) select_stmt)?
    ;

cte_stmt: name AS '(' select_stmt ')';

select_list:
      select_item (COMMA select_item)*
    | STAR
    ;

select_item:
      expr (AS name)?
    | expr name
    | STAR '.' STAR
    ;

from_clause:
      table_ref (COMMA table_ref)*
    ;

table_ref:
      name ('.' name)? (AS name)?
    | '(' select_stmt ')' (AS name)?
    ;

where_expr: boolean_expr ;

group_by_list:
      expr (COMMA expr)*
    ;

having_expr: boolean_expr ;

order_by_list:
      expr (ASC | DESC)? (COMMA expr (ASC | DESC)?)*
    ;

limit_clause:
      INTEGER_LITERAL
    | INTEGER_LITERAL ',' INTEGER_LITERAL
    ;

// ── INSERT ──
insert_stmt:
      INSERT INTO table_ref (column_list)?
      (VALUES '(' (expr (',' expr)*)? ')' (',' '(' (expr (',' expr)*)? ')')*)?
      | select_stmt
    ;

column_list:
      name (COMMA name)*
    ;

// ── CREATE TABLE ──
create_table_stmt:
      CREATE TABLE (IF NOT EXISTS)? table_ref
      '(' column_def (COMMA column_def)* ')'
      (ENGINE = name)?
      (ORDER BY expr)?
      (PARTITION BY expr)?
      (PRIMARY KEY expr)?
      (SAMPLE BY expr)?
    ;

column_def:
      name data_type (NULL | NOT NULL)? (DEFAULT expr)? (COMMENT STRING_LITERAL)?
    ;

// ── DROP TABLE ──
drop_table_stmt:
      DROP TABLE (IF EXISTS)? table_ref
    ;

// ── ALTER TABLE ──
alter_table_stmt:
      ALTER TABLE table_ref
      (ADD COLUMN column_def | DROP COLUMN name | MODIFY COLUMN column_def | ...)
    ;

// ── Expression grammar ──
expr:
      literal
    | name ('.' name)*
    | '(' expr ')'
    | '(' select_stmt ')'
    | expr ('*' | '/' | '%') expr
    | expr ('+' | '-') expr
    | expr (('=' | '!=' | '<' | '>' | '<=' | '>=') expr)?
    | expr (AND | OR) expr
    | expr (NOT | '+' | '-')? expr
    | name '(' (expr (COMMA expr)*)? ')'
    | name '(' DISTINCT expr (COMMA expr)* ')'
    | expr IN '(' (expr (COMMA expr)*)* ')'
    | expr IN select_stmt
    | expr BETWEEN expr AND expr
    | expr IS (NOT)? NULL
    | expr LIKE expr
    | CASE expr? WHEN expr THEN expr (WHEN expr THEN expr)* (ELSE expr)? END
    | CAST '(' expr AS data_type ')'
    | '[' expr ']'
    | ARRAY '[' (expr (',' expr)*)* ']'
    | function_expr
    ;

function_expr:
      name '(' (expr (COMMA expr)*)? ')'
    ;

literal:
      INTEGER_LITERAL
    | FLOAT_LITERAL
    | STRING_LITERAL
    | TRUE
    | FALSE
    | NULL
    ;

data_type:
      name
    | name '(' INT_LITERAL (',' INT_LITERAL)* ')'
    | name '<' data_type (',' data_type)* '>'
    ;

name: IDENTIFIER | STRING_LITERAL ;

// ── Whitespace and comments ──
WS: [ \t\r\n]+ -> skip ;
LINE_COMMENT: '--' ~[\r\n]* -> skip ;
BLOCK_COMMENT: '/*' .*? '*/' -> skip ;
*/
