import struct
RT={1:'S',2:'CB',3:'T',4:'Raw',5:'SB',6:'U',7:'URaw',8:'USB',9:'USBc'}
def psv(blob, off, size):
    rsize=struct.unpack_from('<I',blob,off)[0]; info=blob[off+4:off+4+rsize]
    nt=struct.unpack_from('<III',info,36) if rsize>=48 else None
    p=off+4+rsize; n=struct.unpack_from('<I',blob,p)[0]; p+=4; res=[]
    if n:
        bsz=struct.unpack_from('<I',blob,p)[0]; p+=4
        for i in range(n):
            t,sp,lo,hi=struct.unpack_from('<IIII',blob,p+i*bsz)
            kind=struct.unpack_from('<I',blob,p+i*bsz+16)[0] if bsz>=24 else None
            res.append((RT.get(t,t),sp,lo,hi,kind))
    return nt,res
