package org.mahoyo.port;

import com.github.icedland.iced.x86.*;
import com.github.icedland.iced.x86.dec.Decoder;
import java.io.IOException;
import java.util.*;

/** Strict data interpreter. No native code is executed and no host calls are available. */
final class SteamAccessors {
    static final long BASE=0x140000000L;
    private final SteamPe pe;
    private final Map<Integer,Instruction> decoded=new HashMap<>();
    private final long[] registers=new long[16],defined=new long[16];
    private boolean zero,sign,carry,overflow,flagsValid;
    SteamAccessors(SteamPe pe){this.pe=pe;}
    Instruction instruction(int rva)throws IOException{
        Instruction ins=decoded.get(rva);if(ins==null){ins=new Decoder(64,pe.rva(rva,15),BASE+rva).decode();if(ins.isInvalid()||ins.hasSegmentPrefix()||ins.getLockPrefix()||ins.getRepPrefix())throw rejected();decoded.put(rva,ins);}return ins;
    }
    private static IOException rejected(){return new IOException("Unsupported Steam metadata accessor");}
    private static long mask(int bytes){return bytes==8?-1L:(1L<<(bytes*8))-1;}
    private static int registerIndex(int reg)throws IOException{
        if(reg>=Register.RAX&&reg<=Register.R15)return reg-Register.RAX;
        if(reg>=Register.EAX&&reg<=Register.R15D)return reg-Register.EAX;
        if(reg>=Register.AX&&reg<=Register.R15W)return reg-Register.AX;
        if(reg>=Register.AL&&reg<=Register.BL)return reg-Register.AL;
        if(reg>=Register.SPL&&reg<=Register.R15L)return reg-Register.SPL+4;
        throw rejected();
    }
    private static int registerSize(int reg)throws IOException{registerIndex(reg);return Register.getSize(reg);}
    private long reg(int reg)throws IOException{if(reg==Register.NONE)return 0;int index=registerIndex(reg);long mask=mask(registerSize(reg));if((defined[index]&mask)!=mask)throw rejected();return registers[index]&mask;}
    private void reg(int reg,long value)throws IOException{int index=registerIndex(reg),size=registerSize(reg);long mask=mask(size);registers[index]=size>=4?value&mask:(registers[index]&~mask)|(value&mask);defined[index]=size>=4?-1:defined[index]|mask;}
    private long address(Instruction ins)throws IOException{
        if(ins.getMemoryBase()==Register.RIP)return ins.ipRelativeMemoryAddress();
        return reg(ins.getMemoryBase())+reg(ins.getMemoryIndex())*ins.getMemoryIndexScale()+ins.getMemoryDisplacement64();
    }
    private int size(Instruction ins,int op)throws IOException{return ins.getOpKind(op)==OpKind.REGISTER?registerSize(ins.getOpRegister(op)):MemorySize.getSize(ins.getMemorySize());}
    private long read(Instruction ins,int op)throws IOException{
        int kind=ins.getOpKind(op);if(kind==OpKind.REGISTER)return reg(ins.getOpRegister(op));
        if(kind==OpKind.MEMORY){long rva=address(ins)-BASE;if(rva<0||rva>Integer.MAX_VALUE)throw rejected();return pe.integer((int)rva,MemorySize.getSize(ins.getMemorySize()));}
        if(kind==OpKind.NEAR_BRANCH64)return ins.getNearBranch64();
        if(kind>=OpKind.IMMEDIATE8&&kind<=OpKind.IMMEDIATE32TO64)return ins.getImmediate(op);
        throw rejected();
    }
    private void put(Instruction ins,long value)throws IOException{if(ins.getOp0Kind()!=OpKind.REGISTER)throw rejected();reg(ins.getOp0Register(),value);}
    private void flags(long a,long b,long result,int bytes,boolean subtract){long mask=mask(bytes),top=1L<<(bytes*8-1);a&=mask;b&=mask;result&=mask;zero=result==0;sign=(result&top)!=0;carry=subtract?Long.compareUnsigned(a,b)<0:Long.compareUnsigned(result,a)<0;overflow=((subtract?(a^b):~(a^b))&(a^result)&top)!=0;flagsValid=true;}
    int loop(int effect)throws IOException{
        if(effect<1||effect>8372)throw rejected();Arrays.fill(registers,0);Arrays.fill(defined,0);registers[1]=effect;defined[1]=0xffff;flagsValid=false;
        long ip=BASE+0xa44d0;
        for(int steps=0;steps<10000;steps++){
            if(ip<BASE+0xa44d0||ip>=BASE+0xa4c58)throw rejected();Instruction ins=instruction((int)(ip-BASE));ip=ins.getNextIP();if(ip>BASE+0xa4c58)throw rejected();int op=ins.getMnemonic();
            if(op==Mnemonic.RET)return (int)reg(Register.EAX);
            if(op==Mnemonic.MOV||op==Mnemonic.MOVZX||op==Mnemonic.MOVSX||op==Mnemonic.MOVSXD){long value=read(ins,1);if(op==Mnemonic.MOVSX||op==Mnemonic.MOVSXD){int shift=64-size(ins,1)*8;value=value<<shift>>shift;}put(ins,value);}
            else if(op==Mnemonic.LEA)put(ins,address(ins));
            else if(op==Mnemonic.XOR&&ins.getOp0Kind()==OpKind.REGISTER&&ins.getOp1Kind()==OpKind.REGISTER&&ins.getOp0Register()==ins.getOp1Register()){put(ins,0);flags(0,0,0,size(ins,0),false);}
            else if(op==Mnemonic.CMP||op==Mnemonic.SUB||op==Mnemonic.ADD||op==Mnemonic.DEC){long a=read(ins,0),b=op==Mnemonic.DEC?1:read(ins,1);boolean subtract=op!=Mnemonic.ADD,oldCarry=carry,valid=flagsValid;long result=subtract?a-b:a+b;flags(a,b,result,size(ins,0),subtract);if(op==Mnemonic.DEC){if(!valid)throw rejected();carry=oldCarry;}if(op!=Mnemonic.CMP)put(ins,result);}
            else if(op==Mnemonic.JMP)ip=read(ins,0);
            else {if(!flagsValid)throw rejected();boolean take;
                switch(op){case Mnemonic.JE:take=zero;break;case Mnemonic.JNE:take=!zero;break;case Mnemonic.JA:take=!carry&&!zero;break;case Mnemonic.JG:take=!zero&&sign==overflow;break;default:throw rejected();}
                if(take)ip=read(ins,0);
            }
        }throw rejected();
    }
    static final class Archive {
        final byte[] rows=new byte[74*80];final List<int[]> flags=new ArrayList<>();
        long value(int row,int offset,int size){long value=0;for(int i=0;i<size;i++)value|=(rows[row*80+offset+i]&255L)<<(8*i);return value;}
    }
    private static final long STACK=0x1000000L,CATALOG=BASE+0x3c3a60;
    private byte[] localRead(long address,int length,Archive out,byte[] stack)throws IOException{
        if(address>=STACK&&address-STACK<=stack.length-length)return Arrays.copyOfRange(stack,(int)(address-STACK),(int)(address-STACK)+length);
        if(address>=CATALOG&&address-CATALOG<=out.rows.length-length)return Arrays.copyOfRange(out.rows,(int)(address-CATALOG),(int)(address-CATALOG)+length);
        if(address<BASE||address-BASE>Integer.MAX_VALUE)throw rejected();return pe.rva((int)(address-BASE),length);
    }
    private void localWrite(long address,byte[] bytes,Archive out,byte[] stack)throws IOException{
        if(address>=STACK&&address-STACK<=stack.length-bytes.length)System.arraycopy(bytes,0,stack,(int)(address-STACK),bytes.length);
        else if(address>=CATALOG&&address-CATALOG<=out.rows.length-bytes.length)System.arraycopy(bytes,0,out.rows,(int)(address-CATALOG),bytes.length);
        else throw rejected();
    }
    private static byte[] little(long value,int size){byte[] bytes=new byte[size];for(int i=0;i<size;i++)bytes[i]=(byte)(value>>>(i*8));return bytes;}
    private static long little(byte[] bytes){long value=0;for(int i=0;i<bytes.length;i++)value|=(bytes[i]&255L)<<(i*8);return value;}
    Archive archive()throws IOException{
        Archive out=new Archive();System.arraycopy(pe.rva(0x3c3a60,out.rows.length),0,out.rows,0,out.rows.length);byte[] stack=new byte[4096];byte[][] vectors={new byte[16],new byte[16]};Arrays.fill(registers,0);Arrays.fill(defined,-1L);reg(Register.RSP,STACK+2048);int exits=0,cookies=0;
        long ip=BASE+0x1e50;
        for(int steps=0;steps<2000;steps++){
            if(ip<BASE+0x1e50||ip>BASE+0x40bd)throw rejected();Instruction ins=instruction((int)(ip-BASE));ip=ins.getNextIP();int op=ins.getMnemonic();
            if(op==Mnemonic.RET){if(out.flags.size()!=73||exits!=1||cookies!=1||out.value(73,0,4)!=0xffffffffL)throw rejected();return out;}
            if(op==Mnemonic.NOP)continue;
            if(op==Mnemonic.CALL){long target=ins.getNearBranch64()-BASE;
                if(target==0x143b0){long dest=reg(Register.RCX)-CATALOG-56,first=reg(Register.RDX),last=reg(Register.R8);if(dest!=out.flags.size()*80L||last-first<4||last-first>64||(last-first)%4!=0)throw rejected();int[] flags=new int[(int)(last-first)/4];for(int i=0;i<flags.length;i++)flags[i]=(int)little(localRead(first+i*4L,4,out,stack));out.flags.add(flags);reg(Register.RAX,reg(Register.RCX));}
                else if(target==0x17da04)exits++;
                else if(target==0x17d5c0)cookies++;
                else throw rejected();continue;
            }
            if(op==Mnemonic.PUSH){long sp=reg(Register.RSP)-8;localWrite(sp,little(reg(ins.getOp0Register()),8),out,stack);reg(Register.RSP,sp);}
            else if(op==Mnemonic.POP){long sp=reg(Register.RSP);reg(ins.getOp0Register(),little(localRead(sp,8,out,stack)));reg(Register.RSP,sp+8);}
            else if(op==Mnemonic.MOV){long value=ins.getOp1Kind()==OpKind.MEMORY?little(localRead(address(ins),MemorySize.getSize(ins.getMemorySize()),out,stack)):read(ins,1);if(ins.getOp0Kind()==OpKind.MEMORY)localWrite(address(ins),little(value,MemorySize.getSize(ins.getMemorySize())),out,stack);else put(ins,value);}
            else if(op==Mnemonic.LEA)put(ins,address(ins));
            else if(op==Mnemonic.XOR)put(ins,read(ins,0)^read(ins,1));
            else if(op==Mnemonic.ADD||op==Mnemonic.SUB)put(ins,read(ins,0)+(op==Mnemonic.ADD?1:-1)*read(ins,1));
            else if(op==Mnemonic.XORPS){if(ins.getOp0Register()!=Register.XMM0||ins.getOp1Register()!=Register.XMM0)throw rejected();Arrays.fill(vectors[0],(byte)0);}
            else if(op==Mnemonic.MOVDQA||op==Mnemonic.MOVDQU){if(ins.getOp0Kind()==OpKind.MEMORY){int r=ins.getOp1Register()-Register.XMM0;if(r<0||r>=vectors.length)throw rejected();localWrite(address(ins),vectors[r],out,stack);}else{int r=ins.getOp0Register()-Register.XMM0;if(r<0||r>=vectors.length)throw rejected();vectors[r]=localRead(address(ins),16,out,stack);}}
            else throw rejected();
        }throw rejected();
    }
}
