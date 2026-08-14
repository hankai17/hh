//B = 'x'   @ { puts("finish B"); }
//export A = B     @ { puts("finish A"); }

export main =
  ( ( 'x' % { puts("leave x"); } ) % { puts("leave 内层括号"); } 'y') % { puts("leave 整句"); }

//( ( 'x'                        )                               'y')
