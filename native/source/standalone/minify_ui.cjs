/* Build-time only. No property mangling: HTML IDs, API and global contracts stay. */
const fs=require('fs');
const {minify}=require(process.env.MTTL_TERSER_ROOT);
(async()=>{const result=await minify(fs.readFileSync(0,'utf8'),{compress:{passes:2},mangle:true,format:{comments:false},ecma:2020});if(!result.code)throw Error('Empty minification');process.stdout.write(result.code);})().catch(error=>{console.error(error);process.exitCode=1;});
