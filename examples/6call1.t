B = 'b'

//export S = ('a' !B 'x') | ('c' !B 'y')    // ab[xy] 均可

//export S = ('a' &B 'x') | ('c' &B 'y')

export A = ('a' &B) | ('a' 'c')
