#include "xevan-gate.h"

#if !defined(XEVAN_8WAY) && !defined(XEVAN_4WAY)

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "algo/blake/sph_blake.h"
#include "algo/bmw/sph_bmw.h"
#include "algo/jh/sph_jh.h"
#include "algo/keccak/sph_keccak.h"
#include "algo/skein/sph_skein.h"
#include "algo/shavite/sph_shavite.h"
#include "algo/luffa/luffa_for_sse2.h"
#include "algo/hamsi/sph_hamsi.h"
#include "algo/fugue/sph_fugue.h"
#include "algo/shabal/sph_shabal.h"
#include "algo/whirlpool/sph_whirlpool.h"
#include "algo/haval/sph-haval.h"
#include "algo/simd/nist.h"
#include "algo/cubehash/cubehash_sse2.h"
#include <openssl/sha.h>
#if defined(__AES__)
  #include "algo/groestl/aes_ni/hash-groestl.h"
  #include "algo/echo/aes_ni/hash_api.h"
#else
  #include "algo/groestl/sph_groestl.h"
  #include "algo/echo/sph_echo.h"
#endif

typedef struct {
        sph_blake512_context    blake;
        sph_bmw512_context      bmw;
        sph_skein512_context    skein;
        sph_jh512_context       jh;
        sph_keccak512_context   keccak;
        hashState_luffa         luffa;
        cubehashParam           cubehash;
        sph_shavite512_context  shavite;
        hashState_sd            simd;
        sph_hamsi512_context    hamsi;
        sph_fugue512_context    fugue;
        sph_shabal512_context   shabal;
        sph_whirlpool_context   whirlpool;
        SHA512_CTX              sha512;
        sph_haval256_5_context  haval;
#if defined(__AES__)
        hashState_echo          echo;
        hashState_groestl       groestl;
#else
	sph_groestl512_context  groestl;
        sph_echo512_context     echo;
#endif
} xevan_ctx_holder;

xevan_ctx_holder xevan_ctx __attribute__ ((aligned (64)));

void init_xevan_ctx()
{
        sph_blake512_init(&xevan_ctx.blake);
        sph_bmw512_init(&xevan_ctx.bmw);
        sph_skein512_init(&xevan_ctx.skein);
        sph_jh512_init(&xevan_ctx.jh);
        sph_keccak512_init(&xevan_ctx.keccak);
        init_luffa( &xevan_ctx.luffa, 512 );
        cubehashInit( &xevan_ctx.cubehash, 512, 16, 32 );
        sph_shavite512_init( &xevan_ctx.shavite );
        init_sd( &xevan_ctx.simd, 512 );
        sph_hamsi512_init( &xevan_ctx.hamsi );
        sph_fugue512_init( &xevan_ctx.fugue );
        sph_shabal512_init( &xevan_ctx.shabal );
        sph_whirlpool_init( &xevan_ctx.whirlpool );
        SHA512_Init( &xevan_ctx.sha512 );
        sph_haval256_5_init(&xevan_ctx.haval);
#if defined(__AES__)
        init_groestl( &xevan_ctx.groestl, 64 );
        init_echo( &xevan_ctx.echo, 512 );
#else
	sph_groestl512_init( &xevan_ctx.groestl );
        sph_echo512_init( &xevan_ctx.echo );
#endif
};

void xevan_hash(void *output, const void *input)
{
   uint32_t _ALIGN(64) hash[32]; // 128 bytes required
	const int dataLen = 128;
   xevan_ctx_holder ctx __attribute__ ((aligned (64)));
   memcpy( &ctx, &xevan_ctx, sizeof(xevan_ctx) );

   sph_blake512( &ctx.blake, input, 80 );
   sph_blake512_close( &ctx.blake, hash );
	memset(&hash[16], 0, 64);

	sph_bmw512(&ctx.bmw, hash, dataLen);
	sph_bmw512_close(&ctx.bmw, hash);

#if defined(__AES__)
   update_and_final_groestl( &ctx.groestl, (char*)hash,
                                     (const char*)hash, dataLen*8 );
#else
	sph_groestl512(&ctx.groestl, hash, dataLen);
	sph_groestl512_close(&ctx.groestl, hash);
#endif

	sph_skein512(&ctx.skein, hash, dataLen);
	sph_skein512_close(&ctx.skein, hash);

	sph_jh512(&ctx.jh, hash, dataLen);
	sph_jh512_close(&ctx.jh, hash);

	sph_keccak512(&ctx.keccak, hash, dataLen);
	sph_keccak512_close(&ctx.keccak, hash);

   update_and_final_luffa( &ctx.luffa, (BitSequence*)hash,
                                 (const BitSequence*)hash, dataLen );

   cubehashUpdateDigest( &ctx.cubehash, (byte*)hash,
                                 (const byte*) hash, dataLen );

	sph_shavite512(&ctx.shavite, hash, dataLen);
	sph_shavite512_close(&ctx.shavite, hash);

   update_final_sd( &ctx.simd, (BitSequence *)hash,
                         (const BitSequence *)hash, dataLen*8 );

#if defined(__AES__)
   update_final_echo( &ctx.echo, (BitSequence *) hash,
                           (const BitSequence *) hash, dataLen*8 );
#else
	sph_echo512(&ctx.echo, hash, dataLen);
	sph_echo512_close(&ctx.echo, hash);
#endif

	sph_hamsi512(&ctx.hamsi, hash, dataLen);
	sph_hamsi512_close(&ctx.hamsi, hash);

	sph_fugue512(&ctx.fugue, hash, dataLen);
	sph_fugue512_close(&ctx.fugue, hash);

	sph_shabal512(&ctx.shabal, hash, dataLen);
	sph_shabal512_close(&ctx.shabal, hash);

	sph_whirlpool(&ctx.whirlpool, hash, dataLen);
	sph_whirlpool_close(&ctx.whirlpool, hash);

   SHA512_Update( &ctx.sha512, hash, dataLen );
   SHA512_Final( (unsigned char*) hash, &ctx.sha512 );

	sph_haval256_5(&ctx.haval,(const void*) hash, dataLen);
	sph_haval256_5_close(&ctx.haval, hash);

	memset(&hash[8], 0, dataLen - 32);

   memcpy( &ctx, &xevan_ctx, sizeof(xevan_ctx) );

	sph_blake512(&ctx.blake, hash, dataLen);
	sph_blake512_close(&ctx.blake, hash);

	sph_bmw512(&ctx.bmw, hash, dataLen);
	sph_bmw512_close(&ctx.bmw, hash);

#if defined(__AES__)
   update_and_final_groestl( &ctx.groestl, (char*)hash,
                              (const BitSequence*)hash, dataLen*8 );
#else
	sph_groestl512(&ctx.groestl, hash, dataLen);
   sph_groestl512_close(&ctx.groestl, hash);
#endif

	sph_skein512(&ctx.skein, hash, dataLen);
	sph_skein512_close(&ctx.skein, hash);

	sph_jh512(&ctx.jh, hash, dataLen);
	sph_jh512_close(&ctx.jh, hash);

	sph_keccak512(&ctx.keccak, hash, dataLen);
	sph_keccak512_close(&ctx.keccak, hash);

   update_and_final_luffa( &ctx.luffa, (BitSequence*)hash,
                                 (const BitSequence*)hash, dataLen );

   cubehashUpdateDigest( &ctx.cubehash, (byte*)hash,
                                 (const byte*) hash, dataLen );

	sph_shavite512(&ctx.shavite, hash, dataLen);
	sph_shavite512_close(&ctx.shavite, hash);

   update_final_sd( &ctx.simd, (BitSequence *)hash,
                         (const BitSequence *)hash, dataLen*8 );

#if defined(__AES__)
   update_final_echo( &ctx.echo, (BitSequence *) hash,
                           (const BitSequence *) hash, dataLen*8 );
#else
   sph_echo512(&ctx.echo, hash, dataLen);
   sph_echo512_close(&ctx.echo, hash);
#endif

	sph_hamsi512(&ctx.hamsi, hash, dataLen);
	sph_hamsi512_close(&ctx.hamsi, hash);

	sph_fugue512(&ctx.fugue, hash, dataLen);
	sph_fugue512_close(&ctx.fugue, hash);

	sph_shabal512(&ctx.shabal, hash, dataLen);
	sph_shabal512_close(&ctx.shabal, hash);

	sph_whirlpool(&ctx.whirlpool, hash, dataLen);
	sph_whirlpool_close(&ctx.whirlpool, hash);

   SHA512_Update( &ctx.sha512, hash, dataLen );
   SHA512_Final( (unsigned char*) hash, &ctx.sha512 );

	sph_haval256_5(&ctx.haval,(const void*) hash, dataLen);
	sph_haval256_5_close(&ctx.haval, hash);

	memcpy(output, hash, 32);
}

int scanhash_xevan( struct work *work, uint32_t max_nonce,
             uint64_t *hashes_done, struct thr_info *mythr)
{
   uint32_t edata[20] __attribute__((aligned(64)));
   uint32_t hash64[8] __attribute__((aligned(64)));
   uint32_t *pdata = work->data;
   uint32_t *ptarget = work->target;
   uint32_t n = pdata[19];
   const uint32_t first_nonce = pdata[19];
   const int thr_id = mythr->id;
   const bool bench = opt_benchmark;

   mm128_bswap32_80( edata, pdata );

   do
   {
      edata[19] = n;
      xevan_hash( hash64, edata );
      if ( unlikely( valid_hash( hash64, ptarget ) && !bench ) )
      {
         pdata[19] = bswap_32( n );
         submit_solution( work, hash64, mythr );
      }
      n++;
   } while ( n < max_nonce && !work_restart[thr_id].restart );
   pdata[19] = n;
   *hashes_done = n - first_nonce;
   return 0;
}

#endif
