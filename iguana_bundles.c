/******************************************************************************
 * Copyright © 2014-2015 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

#include "iguana777.h"

static uint16_t iguana_primes[] = { 65353, 65357, 65371, 65381, 65393, 65407, 65413, 65419, 65423, 65437, 65447, 65449, 65479, 65497, 65519, 65521 };

struct iguana_bloominds { uint16_t inds[8]; };

struct iguana_bloominds iguana_calcbloom(bits256 hash2)
{
    int32_t i,j,k; struct iguana_bloominds bit;
    k = (int32_t)(sizeof(bit)/sizeof(uint16_t)) - 1;
    j = 15;
    for (i=0; i<sizeof(bit)/sizeof(uint16_t); i++,j--,k--)
        bit.inds[i] = (hash2.ushorts[j] % iguana_primes[k]);//, printf("%d ",bit.inds[i]);
    //printf("bit.inds\n");
    return(bit);
}

struct iguana_bloominds iguana_bloomset(struct iguana_info *coin,struct iguana_bloom16 *bloom,int32_t incr,struct iguana_bloominds bit)
{
    int32_t i,alreadyset;
    for (alreadyset=i=0; i<sizeof(bit)/sizeof(uint16_t); i++,bloom+=incr)
    {
        if ( GETBIT(bloom->hash2bits,bit.inds[i]) == 0 )
            SETBIT(bloom->hash2bits,bit.inds[i]);
        else alreadyset++;
    }
    if ( alreadyset == i )
        printf("iguana_bloomset: collision\n");
    return(bit);
}

int32_t iguana_bloomfind(struct iguana_info *coin,struct iguana_bloom16 *bloom,int32_t incr,struct iguana_bloominds bit)
{
    int32_t i;
    coin->bloomsearches++;
    for (i=0; i<sizeof(bit)/sizeof(uint16_t); i++,bloom+=incr)
        if ( GETBIT(bloom->hash2bits,bit.inds[i]) == 0 )
            return(-1);
    coin->bloomhits++;
    return(0);
}

int32_t iguana_bundlescan(struct iguana_info *coin,struct iguana_bundle *bp,bits256 hash2)
{
    int32_t bundlei;
    for (bundlei=0; bundlei<bp->n; bundlei++)
    {
        if ( memcmp(hash2.bytes,bp->hashes[bundlei].bytes,sizeof(hash2)) == 0 )
        {
            //char str[65]; printf("hdrsi.%d scan.%s found %d of %d\n",bp->ramchain.hdrsi,bits256_str(str,hash2),bundlei,bp->n);
            return(bundlei);
        }
    }
    return(-2);
}

struct iguana_bundle *iguana_bundlefind(struct iguana_info *coin,struct iguana_bundle **bpp,int32_t *bundleip,bits256 hash2)
{
    int32_t i; struct iguana_bloominds bit; struct iguana_bundle *bp = *bpp;
    bit = iguana_calcbloom(hash2);
    if ( bp == 0 )
    {
        for (i=coin->bundlescount-1; i>=0; i--)
        {
            if ( (bp= coin->bundles[i]) != 0 )
            {
                if ( iguana_bloomfind(coin,&bp->bloom,0,bit) == 0 )
                {
                    *bpp = bp;
                    if ( (*bundleip= iguana_bundlescan(coin,bp,hash2)) < 0 )
                    {
                        //printf("bloom miss\n");
                        coin->bloomfalse++;
                    }
                    else return(bp);
                } //else printf("no bloom\n");
            }
        }
        *bundleip = -2;
        *bpp = 0;
        return(0);
    }
    else if ( iguana_bloomfind(coin,&bp->bloom,0,bit) == 0 )
    {
        if ( (*bundleip= iguana_bundlescan(coin,bp,hash2)) >= 0 )
        {
            *bpp = bp;
            return(bp);
        } else printf("scan miss\n");
    }
    *bpp = 0;
    *bundleip = -2;
    return(0);
}

bits256 *iguana_bundleihash2p(struct iguana_info *coin,int32_t *isinsidep,struct iguana_bundle *bp,int32_t bundlei)
{
    *isinsidep = 0;
    if ( bundlei >= 0 && bundlei < coin->chain->bundlesize )
    {
        *isinsidep = 1;
        return(&bp->hashes[bundlei]);
    }
    else if ( bundlei == -1 )
        return(&bp->prevbundlehash2);
    else if ( bundlei == coin->chain->bundlesize)
        return(&bp->nextbundlehash2);
    else return(0);
}

int32_t iguana_hash2set(struct iguana_info *coin,char *debugstr,struct iguana_bundle *bp,int32_t bundlei,bits256 newhash2)
{
    int32_t isinside,checki,retval = -1; bits256 *orighash2p = 0; struct iguana_bundle *checkbp; char str[65]; struct iguana_bloominds bit;
    if ( bp->n <= bundlei )
    {
        printf("hash2set.%s [%d] of %d <- %s\n",debugstr,bundlei,bp->n,bits256_str(str,newhash2));
        bp->n = coin->chain->bundlesize;
    }
    if ( bits256_nonz(newhash2) == 0 || (orighash2p= iguana_bundleihash2p(coin,&isinside,bp,bundlei)) == 0 )
    {
        printf("iguana_hash2set warning: bundlei.%d newhash2.%s orighash2p.%p\n",bundlei,bits256_str(str,newhash2),orighash2p);
        getchar();
        return(-1);
    }
    if ( bits256_nonz(*orighash2p) > 0 && memcmp(newhash2.bytes,orighash2p,sizeof(bits256)) != 0 )
    {
        char str2[65],str3[65];
        bits256_str(str2,*orighash2p), bits256_str(str3,newhash2);
        printf("ERRRO iguana_hash2set overwrite [%s] %s with %s\n",debugstr,str2,str3);
        getchar();
    }
    if ( isinside != 0 )
    {
        bit = iguana_calcbloom(newhash2);
        if ( iguana_bloomfind(coin,&bp->bloom,0,bit) < 0 )
        {
            //printf("bloomset (%s)\n",bits256_str(str,newhash2));
            iguana_bloomset(coin,&bp->bloom,0,bit);
            if ( 1 )
            {
                int32_t i;
                if ( iguana_bloomfind(coin,&bp->bloom,0,bit) < 0 )
                {
                    for (i=0; i<8; i++)
                        printf("%d ",bit.inds[i]);
                    printf("cant bloomfind just bloomset\n");
                }
                else
                {
                    *orighash2p = newhash2;
                    checkbp = bp, checki = -2;
                    if ( iguana_bundlefind(coin,&checkbp,&checki,newhash2) == 0 || checki != bundlei )
                    {
                        printf("cant iguana_bundlefind just added.(%s) bundlei.%d %p vs checki.%d %p\n",bits256_str(str,newhash2),bundlei,bp,checki,checkbp);
                    }
                    else if ( (coin->bloomsearches % 100000) == 0 )
                        printf("BLOOM SUCCESS %.2f%% FP.%d/%d collisions.%d\n",100.*(double)coin->bloomhits/coin->bloomsearches,(int32_t)coin->bloomfalse,(int32_t)coin->bloomsearches,(int32_t)coin->collisions);
                }
            }
        } //else printf("bloom found\n");
        retval = 0;
    } else retval = (bundlei >= 0 && bundlei < coin->chain->bundlesize) ? 0 : 1;
    //printf("set [%d] <- %s\n",bundlei,bits256_str(str,newhash2));
    *orighash2p = newhash2;
    return(retval);
}

int32_t iguana_bundlehash2add(struct iguana_info *coin,struct iguana_block **blockp,struct iguana_bundle *bp,int32_t bundlei,bits256 hash2)
{
    struct iguana_block *block =0; struct iguana_bundle *otherbp; //
    int32_t otherbundlei,setval,bundlesize,err = 0;
    if ( blockp != 0 )
        *blockp = 0;
    if ( bp == 0 )
        return(0);
    if ( bits256_nonz(hash2) > 0 && (block= iguana_blockhashset(coin,-1,hash2,1)) != 0 )
    {
        bundlesize = coin->chain->bundlesize;
        if ( bundlei >= bp->n )
        {
            bp->n = bundlesize;//(bundlei < bundlesize-1) ? bundlesize : (bundlei + 1);
        }
        if ( (setval= iguana_hash2set(coin,"blockadd",bp,bundlei,hash2)) == 0 )
        {
            if ( (block->hdrsi != bp->ramchain.hdrsi || block->bundlei != bundlei) && (block->hdrsi != 0 || block->bundlei != 0) )
            {
                printf("blockadd warning: %d[%d] <- %d[%d]\n",block->hdrsi,block->bundlei,bp->ramchain.hdrsi,bundlei);
                err |= 2;
            }
            //char str[65]; printf(">>>>>>>>>>>>>> bundlehash2.(%s) ht.(%d %d)\n",bits256_str(str,hash2),bp->ramchain.bundleheight,bundlei);
            block->hdrsi = bp->ramchain.hdrsi;
            block->bundlei = bundlei;
            block->havebundle = 1;
            otherbp = 0;
            if ( (otherbp= iguana_bundlefind(coin,&otherbp,&otherbundlei,hash2)) != 0 || (bundlei % (bundlesize-1)) == 0)
            {
                if ( bundlei == 0 && (otherbundlei == -2 || otherbundlei == bundlesize-1) )
                {
                    if ( otherbp != 0 && iguana_hash2set(coin,"blockadd0_prev",bp,-1,otherbp->hashes[0]) != 0 )
                        err |= 4;
                    if ( otherbp != 0 && iguana_hash2set(coin,"blockadd0_next",otherbp,bundlesize,bp->hashes[0]) != 0 )
                        err |= 8;
                }
                else if ( bundlei == bundlesize-1 && (otherbundlei == -2 || otherbundlei == 0) )
                {
                    if ( otherbp != 0 && iguana_hash2set(coin,"blockaddL_prev",otherbp,-1,bp->hashes[0]) != 0 )
                        err |= 16;
                    if ( otherbp != 0 && iguana_hash2set(coin,"blockaddL_next",bp,bundlesize,otherbp->hashes[0]) != 0 )
                        err |= 32;
                }
                //else printf("blockadd warning: %d[%d] bloomfound %d[%d]\n",bp->ramchain.hdrsi,bundlei,otherbp!=0?otherbp->ramchain.hdrsi:-1,otherbundlei);
            }
        }
        else if ( setval == 1 )
        {
            if ( bundlei == -1 && iguana_hash2set(coin,"blockadd_m1",bp,-1,hash2) != 0 )
                err |= 4;
            if ( bundlei == bundlesize && iguana_hash2set(coin,"blockaddL_m1",bp,bundlesize,hash2) != 0 )
                err |= 4;
        }
        else if ( setval < 0 )
        {
            printf("neg setval error\n");
            err |= 64;
        }
        if ( err == 0 && blockp != 0 )
            *blockp = block;
    } else err |= 128;
    return(-err);
}

struct iguana_bundle *iguana_bundlecreate(struct iguana_info *coin,int32_t *bundleip,int32_t bundleheight,bits256 bundlehash2)
{
    char str[65]; struct iguana_bundle *bp = 0;
    if ( bits256_nonz(bundlehash2) > 0 )
    {
        bits256_str(str,bundlehash2);
        if ( iguana_bundlefind(coin,&bp,bundleip,bundlehash2) != 0 )
        {
            if ( bp->ramchain.bundleheight >= 0 && bp->ramchain.bundleheight != (bundleheight - *bundleip) )
                printf("bundlecreate warning: bp->bundleheight %d != %d (bundleheight %d - %d bundlei)\n",bp->ramchain.bundleheight,(bundleheight - *bundleip),bundleheight,*bundleip);
            if ( bp->numhashes < bp->n )
                queue_enqueue("hdrsQ",&coin->hdrsQ,queueitem(str),1);
            return(bp);
        }
        bp = mycalloc('b',1,sizeof(*bp));
        bp->n = coin->chain->bundlesize;
        bp->ramchain.hdrsi = coin->bundlescount;
        bp->ramchain.bundleheight = bundleheight;
        iguana_hash2set(coin,"create",bp,0,bundlehash2);
        if ( iguana_bundlehash2add(coin,0,bp,0,bundlehash2) == 0 )
        {
            bp->coin = coin;
            bp->avetime = coin->avetime * 2.;
            coin->bundles[coin->bundlescount] = bp;
            *bundleip = 0;
            printf("ht.%d alloc.[%d] new hdrs.%s %p\n",bp->ramchain.bundleheight,coin->bundlescount,str,bp);
            //if ( bits256_nonz(bundlehash2) > 0 )
               iguana_blockQ(coin,bp,0,bundlehash2,1);
            //if ( bits256_nonz(firstblockhash2) > 0 )
            //    iguana_blockQ(coin,bp,1,firstblockhash2,1);
            coin->bundlescount++; // must be LAST thing done
            iguana_bundlehash2add(coin,0,bp,0,bundlehash2);
            queue_enqueue("hdrsQ",&coin->hdrsQ,queueitem(str),1);
        }
        else
        {
            printf("error adding bundlehash2 bundleheight.%d\n",bundleheight);
            myfree(bp,sizeof(*bp));
            bp = 0;
        }
        return(bp);
    } else printf("cant create bundle with zerohash\n");
    //else printf("iguana_hdrscreate cant find hdr with %s or %s\n",bits256_str(bundlehash2),bits256_str2(firstblockhash2));
    return(0);
}

/*struct iguana_block *iguana_blockadd(struct iguana_info *coin,struct iguana_bundle **bpp,int32_t *bundleip,struct iguana_block *origblock)
{
    struct iguana_block *checkblock,*block = 0; char str[65]; struct iguana_bundle *bp = *bpp;
    int32_t setval,checki,bundlei,bundleheight,bundlesize = coin->chain->bundlesize;
    bundlei = *bundleip;
    *bundleip = -2;
    *bpp = 0;
    if ( origblock == 0 )
        return(0);
    //iguana_blockhashset(coin,-1,origblock->prev_block,1);
    if ( bits256_nonz(origblock->hash2) > 0 && (block= iguana_blockhashset(coin,-1,origblock->hash2,1)) != 0 )
    {
        //printf("blockadd.(%s) -> %d (%s)\n",bits256_str(str,origblock->prev_block),block->height,bits256_str(str2,origblock->hash2));
        if ( bits256_nonz(block->prev_block) == 0 )
            iguana_blockcopy(coin,block,origblock);
        if ( (bp= *bpp) == 0 )
        {
            if ( (bp= iguana_bundlefind(coin,bpp,&bundlei,block->hash2)) == 0 )
            {
                *bpp = 0, bundlei = -2;
                //printf("a bundlefind.(%s) -> bundlei.%d\n",bits256_str(str,block->hash2),*bundleip);
                if ( (bp= iguana_bundlefind(coin,bpp,&bundlei,block->prev_block)) == 0 )
                {
                    //iguana_chainheight(coin,block);
                    //printf("a prev bundlefind.(%s) -> bundlei.%d ht.%d\n",bits256_str(str,block->prev_block),*bundleip,block->height);
                     *bpp = 0;
                    *bundleip = -2;
                    return(block);
                }
                else
                {
                    if ( *bundleip == bundlesize-1 )
                    {
                        printf("b prev bundlefind.(%s) -> bundlei.%d\n",bits256_str(str,block->prev_block),*bundleip);
                        bundleheight = (bp->ramchain.bundleheight >= 0) ? (bp->ramchain.bundleheight + bundlesize) : -1;
                        printf("autocreateA: bundleheight.%d\n",bundleheight);
                        bundlei = -2;
                        *bpp = bp = iguana_bundlecreate(coin,&bundlei,bundleheight,block->hash2);
                        *bpp = 0;
                        *bundleip = -2;
                        return(block);
                    }
                    else if ( bundlei < coin->chain->bundlesize-1 )
                    {
                        bundlei++;
                        if ( bp->n <= bundlei )
                            bp->n = bundlei+1;
                        iguana_hash2set(coin,"add",bp,bundlei,block->hash2);
                        //printf("found prev.%s -> bundlei.%d\n",bits256_str(str,block->prev_block),bundlei);
                    }
                }
            }
            else
            {
               // printf("found bp.%p bundlei.%d\n",bp,bundlei);
            }
        }
        else if ( bundlei < -1 )
        {
            bp = iguana_bundlefind(coin,bpp,&bundlei,block->hash2);
            printf("c bundlefind.(%s) -> bundlei.%d\n",bits256_str(str,block->hash2),bundlei);
        } else printf("last case bundleip %d\n",bundlei);
        *bpp = bp;
        *bundleip = bundlei;
        //printf("bundlei.%d for %s\n",bundlei,bits256_str(str,block->hash2));
        if ( memcmp(bp->hashes[bundlei].bytes,block->hash2.bytes,sizeof(bits256)) != 0 )
            printf("honk? find error %s\n",bits256_str(str,bp->hashes[bundlei])), getchar();
        if ( bp == 0 || bundlei < -1 )
        {
            printf("%s null bp? %p or illegal bundlei.%d block.%p\n",bits256_str(str,block->hash2),bp,bundlei,block);
            return(block);
        }
        if ( (setval= iguana_bundlehash2add(coin,&checkblock,bp,bundlei,block->hash2)) == 0 && checkblock == block )
        {
            if ( bp->blocks[bundlei] != 0 )
            {
                if ( bp->blocks[bundlei] != block )
                    printf("blockadd: error blocks[%d] %p %d != %d %p\n",bundlei,bp->blocks[bundlei],bp->blocks[bundlei]->height,block->height,block);
            } else bp->blocks[bundlei] = block;
            //iguana_bundlehash2add(coin,0,bp,bundlei-1,block->prev_block);
            //printf("setval.%d bp.%p bundlei.%d\n",setval,bp,bundlei);
        }
        else if ( setval > 0 )
        {
            if ( bundlei == bundlesize )
            {
                bundleheight = (bp->ramchain.bundleheight >= 0) ? (bp->ramchain.bundleheight + bundlesize) : -1;
                printf("autocreate: bundleheight.%d\n",bundleheight);
                iguana_bundlecreate(coin,&checki,bundleheight,block->hash2);
            }
            printf("setval.%d bundlei.%d\n",setval,bundlei);
        } else printf("blockadd: error.%d adding hash2, checkblock.%p vs %p\n",setval,checkblock,block);
        //printf("bundleblockadd.[%d] of %d <- %s setval.%d %p\n",bundlei,bp->n,bits256_str(str,block->hash2),setval,block);
    } else printf("bundleblockadd: block.%p error\n",block);
    return(block);
}

struct iguana_block *iguana_bundleblockadd(struct iguana_info *coin,struct iguana_bundle **bpp,int32_t *bundleip,struct iguana_block *origblock)
{
    struct iguana_block *block,*retblock; int32_t i,oldhwm; struct iguana_bundle *bp;
    bits256 *hash2p,hash2; char str[65]; struct iguana_bloominds bit;
    oldhwm = coin->blocks.hwmchain.height;
    *bpp = 0, *bundleip = -2;
    if ( (retblock= iguana_blockadd(coin,bpp,bundleip,origblock)) != 0 )
    {
        block = retblock;
        //iguana_chainextend(coin,block);
        if ( block->height >= 0 && (hash2p= iguana_blockhashptr(coin,coin->blocks.hashblocks)) != 0 )
            *hash2p = block->hash2;
        if ( oldhwm != coin->blocks.hwmchain.height )
        {
            if ( oldhwm < coin->blocks.hashblocks )
                coin->blocks.hashblocks = oldhwm;
            while ( coin->blocks.hashblocks < coin->blocks.hwmchain.height && (hash2p= iguana_blockhashptr(coin,coin->blocks.hashblocks)) != 0 )
            {
                hash2 = *hash2p;
                if ( bits256_nonz(hash2) > 0 && (block= iguana_blockfind(coin,hash2)) != 0 )
                {
                    if ( hash2p == 0 )
                    {
                        printf("iguana_bundleblockadd B cant find coin->blocks.hashblocks %d\n",coin->blocks.hashblocks);
                        break;
                    }
                    *hash2p = hash2;
                    for (i=0; i<coin->bundlescount; i++)
                    {
                        if ( (bp= coin->bundles[i]) != 0 )
                        {
                            if ( coin->blocks.hashblocks >= bp->ramchain.bundleheight && coin->blocks.hashblocks < bp->ramchain.bundleheight+bp->n )
                            {
                                bit = iguana_calcbloom(block->hash2);
                                if ( iguana_bloomfind(coin,&bp->bloom,0,bit) < 0 )
                                    iguana_bloomset(coin,&bp->bloom,0,bit);
                                break;
                            }
                        }
                    }
                    //printf("ht.%d %s %p\n",block->height,bits256_str(str,hash2),hash2p);
                    bp = 0;
                    *bundleip = -2;
                    iguana_blockadd(coin,&bp,bundleip,block);
                    bp = 0;
                    *bundleip = -2;
                    if ( iguana_bundlefind(coin,&bp,bundleip,block->hash2) == 0 )
                    {
                        printf("iguana_bundleblockadd A cant find just added.%s bundlei.%d\n",bits256_str(str,hash2),*bundleip);
                        bp = 0;
                        *bundleip = -2;
                        iguana_bundlefind(coin,&bp,bundleip,block->hash2);
                        break;
                    }
                    coin->blocks.hashblocks++;
                    block = 0;
                }
                else
                {
                    //printf("break loop block.%p %s coin->blocks.hashblocks %d vs %d\n",block,bits256_str(str,hash2),coin->blocks.hashblocks,coin->blocks.hwmheight);
                    break;
                }
            }
        }
    } else printf("iguana_bundleblockadd returns null\n");
    return(retblock);
}*/

char *iguana_bundledisp(struct iguana_info *coin,struct iguana_bundle *prevbp,struct iguana_bundle *bp,struct iguana_bundle *nextbp,int32_t m)
{
    static char line[1024];
    line[0] = 0;
    if ( bp == 0 )
        return(line);
    if ( prevbp != 0 )
    {
        if ( memcmp(prevbp->hashes[0].bytes,bp->prevbundlehash2.bytes,sizeof(bits256)) == 0 )
        {
            if ( memcmp(prevbp->nextbundlehash2.bytes,bp->hashes[0].bytes,sizeof(bits256)) == 0 )
                sprintf(line+strlen(line),"<->");
            else sprintf(line+strlen(line),"<-");
        }
        else if ( memcmp(prevbp->nextbundlehash2.bytes,bp->hashes[0].bytes,sizeof(bits256)) == 0 )
            sprintf(line+strlen(line),"->");
    }
    sprintf(line+strlen(line),"(%d:%d)",bp->ramchain.hdrsi,m);
    if ( nextbp != 0 )
    {
        if ( memcmp(nextbp->hashes[0].bytes,bp->nextbundlehash2.bytes,sizeof(bits256)) == 0 )
        {
            if ( memcmp(nextbp->prevbundlehash2.bytes,bp->hashes[0].bytes,sizeof(bits256)) == 0 )
                sprintf(line+strlen(line),"<->");
            else sprintf(line+strlen(line),"->");
        }
        else if ( memcmp(nextbp->prevbundlehash2.bytes,bp->hashes[0].bytes,sizeof(bits256)) == 0 )
            sprintf(line+strlen(line),"<-");
    }
    return(line);
}

void iguana_bundlestats(struct iguana_info *coin,char *str)
{
    int32_t i,dispflag,checki,bundlei,numbundles,numdone,numrecv,numhashes,numissued,numemit,numactive,totalrecv = 0;
    struct iguana_bundle *bp; struct iguana_block *block; int64_t datasize,estsize = 0; char fname[1024];
    //iguana_chainextend(coin,iguana_blockfind(coin,coin->blocks.hwmchain));
    if ( queue_size(&coin->blocksQ) == 0 )
        iguana_blockQ(coin,0,-1,coin->blocks.hwmchain.hash2,0);
    dispflag = (rand() % 100) == 0;
    numbundles = numdone = numrecv = numhashes = numissued = numemit = numactive = 0;
    for (i=0; i<coin->bundlescount; i++)
    {
        if ( (bp= coin->bundles[i]) != 0 )
        {
            bp->numhashes = 0;
            numbundles++;
            if ( bp->numrecv >= bp->n )
                numdone++;
            for (numrecv=datasize=bundlei=0; bundlei<bp->n; bundlei++)
            {
                if ( bits256_nonz(bp->hashes[bundlei]) == 0 )
                    continue;
                bp->numhashes++;
                if ( (block= iguana_blockfind(coin,bp->hashes[bundlei])) != 0 && block->recvlen != 0 )
                {
                    numrecv++;
                    numissued++;
                    if ( block->ipbits != 0 )
                    {
                        iguana_memreset(&coin->blockMEM);
                        if ( iguana_peertxdata(coin,&checki,fname,&coin->blockMEM,block->ipbits,block->hash2) != 0 && checki == bundlei )
                            datasize += block->recvlen;
                        else block->recvlen = block->ipbits = 0;
                    }
                }
                else if ( bp->issued[bundlei] > SMALLVAL )
                    numissued++;
            }
            numhashes += bp->numhashes;
            bp->numrecv = numrecv;
            bp->datasize = datasize;
            if ( bp->emitfinish != 0 )
                numemit++;
            else if ( numrecv > 0 )
            {
                bp->estsize = ((uint64_t)datasize * bp->n) / numrecv;
                estsize += bp->estsize;
                numactive++;
                if ( dispflag != 0 )
                    printf("(%d %d) ",i,bp->numrecv);
                if ( bp->numrecv == bp->n && bp->emitfinish == 0 )
                {
                    bp->emitfinish = 1;
                    iguana_emitQ(coin,bp);
                }
                if ( numrecv > bp->n*.98 )
                {
                    if ( numrecv > bp->n-3 )
                        bp->threshold = bp->avetime;
                    else bp->threshold = bp->avetime * 2;
                } else bp->threshold = bp->avetime * 5;
                bp->metric = bp->threshold / (1. + fabs((bp->n - bp->numrecv) * sqrt(bp->estsize - bp->datasize)));
            } else bp->threshold = 10000., bp->metric = 0.;
            totalrecv += numrecv;
        }
    }
    char str2[65];
    sprintf(str,"N[%d] d.%d p.%d g.%d A.%d h.%d i.%d r.%d E.%d:%d M.%d long.%d est.%d %s",coin->bundlescount,numdone,coin->numpendings,numbundles,numactive,numhashes,numissued,totalrecv,numemit,coin->numemitted,coin->blocks.hwmchain.height,coin->longestchain,coin->MAXBUNDLES,mbstr(str2,estsize));
    coin->activebundles = numactive;
    coin->estsize = estsize;
    coin->numrecv = totalrecv;
}
