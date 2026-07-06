# Currently unable to get the top-N using the LIMIT keyword

e.g SELECT customer_id, age FROM customers LIMIT 3 
returns {'rows': 9,
 'columns': ['customer_id', 'age'],
 'data': [[4, 28],
  [4, 28],
  [5, 22],
  [6, 27],
  [7, 28],
  [8, 21],
  [9, 35],
  [10, 23],
  [11, 11]],
 'duration_ms': 0.19,
 'bytes_read': 0,
 'bytes_written': 0}