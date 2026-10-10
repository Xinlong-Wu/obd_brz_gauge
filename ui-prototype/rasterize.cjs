const fs=require('fs'),path=require('path');
const sharp=require('sharp');
const d=__dirname;
(async()=>{const files=fs.readdirSync(path.join(d,'vectors')).filter(n=>n.endsWith('.svg'));for(const f of files)await sharp(path.join(d,'vectors',f)).resize(720).png().toFile(path.join(d,'previews',f.replace('.svg','.png')));await sharp(path.join(d,'overview.svg')).resize(1800).png().toFile(path.join(d,'overview.png'));console.log('Rendered '+files.length+' previews and overview');})().catch(e=>{console.error(e);process.exit(1)});
