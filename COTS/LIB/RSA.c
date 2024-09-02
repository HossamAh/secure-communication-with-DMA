#include "Std_Types.h"
#include "stdio.h"
#include "math.h"
// extern uint16 p,q,k;
uint16 p,q,k;
static uint32 gcd(uint32 e,uint32 phi)
{
    uint32 temp;
    while(1)
    {
        temp = e%phi;
        if(temp==0)
            return phi;
        e = phi;
        phi = temp;
    }
}
static void GeneratePublicKey(uint32* n,uint32* e)//for encryption
{
    /*p and q both are prime numbers*/
    *n = p*q;
    uint32 phi = (p-1)*(q-1);
    *e =2;
    /*e must be co-prime to phi then gcd must equal 1*/
    /*e from 1 to phi so e must be <phi*/
    while (*e<phi)
    {
        if(gcd(*e,phi)==1)
            break;
        (*e)++;
    }
}

static uint16 GeneratePrivateKey(uint32* d,uint32* n)//for decryption
{
    uint32 phi = (p-1)*(q-1);
    uint32 e;
    GeneratePublicKey(n,&e);
    *d = (k*phi+1)/(e);
}

void RSAencryptMsg(uint8* msg,uint8* encryptedMsg)
{
    /*encryptedMsg's size is 4 times msg's size*/
    uint8 itr=0;
    uint32 e , n;
    GeneratePublicKey(&n,&e);
    uint16 tempe=e;
    while(msg[itr])
    {
        tempe=e;
        uint64 number=1;
        /*encrypt*/
        while (tempe--) {
            number *= msg[itr];
            number %= n;
        }
        void* byte_of_word = (uint8*)&number;
        encryptedMsg[itr*4 +0]=*((uint8*)byte_of_word);
        encryptedMsg[itr*4 +1]=*((uint8*)byte_of_word+1);
        encryptedMsg[itr*4 +2]=*((uint8*)byte_of_word+2);
        encryptedMsg[itr*4 +3]=*((uint8*)byte_of_word+3);
        itr++;
    }
    encryptedMsg[itr]='\0';
}

void RSAdecryptMsg(uint8* msg,uint8* decryptedMsg)
{
    uint8 itr=0;
    uint32 d , n;
    GeneratePrivateKey(&d,&n);
    uint16 tempd=d;
    while(msg[itr])
    {
        tempd=d;
        uint64 encryptedNubmer=0;
        uint8* ptr = (uint8*)&encryptedNubmer;
        ptr[4*itr +0] = msg[itr];
        ptr[4*itr +1] = msg[itr+1];
        ptr[4*itr +2] = msg[itr+2];
        ptr[4*itr +3] = msg[itr+3];
        uint64 ch=1;
        /*encrypt*/
        while (tempd--) {
            ch *= encryptedNubmer;
            ch %= n;
        }
        decryptedMsg[itr]=(uint8)ch;
        itr+=4;
    }
    decryptedMsg[itr]='\0';
}

int main(void)
{
    uint8 init_msg[15] ="hi from hossam";
    uint8 encryptedMsg[15*4];
    uint8 decryptedMsg[15];
    
    p=53;
    q=59;
    k=2;
    RSAencryptMsg(init_msg,encryptedMsg);
    printf("%s",encryptedMsg);
    RSAdecryptMsg(encryptedMsg,decryptedMsg);
    printf("%s",decryptedMsg);
    
    return 0;
}