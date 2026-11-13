const fs=require('fs'),vm=require('vm'),assert=require('assert');
for(const [file,height,step] of [['board-preview.html',170,26],['cyd-preview/board-preview.html',240,40]]){
 const canvas={style:{},classList:{toggle(){}},getBoundingClientRect:()=>({left:0,top:0,width:320,height}),getContext:()=>({fillRect(){},drawImage(){}})};
 const control={setAttribute(){}};
 const context=vm.createContext({document:{querySelector:s=>s==='canvas'?canvas:control},Image:class{set src(v){this.onload?.();}}});
 const source=fs.readFileSync(file,'utf8').match(/<script>([\s\S]*?)<\/script>/)[1];
 vm.runInContext(source,context);
 const state=()=>vm.runInContext('({view,brightness,brightnessDraft,chosen,north,inverted})',context);
 const click=(x,y)=>canvas.onclick({clientX:x,clientY:y});
 const recentY=32+3*step+5,presetY=height===240?140:94,sliderY=height===240?105:70;
 click(110,height-15);assert.equal(state().view,'recent');
 click(160,recentY);assert.equal(state().view,'brightness');
 click(160,presetY+12);assert.equal(state().brightnessDraft,50);assert.equal(state().brightness,100);
 click(285,14);assert.equal(state().view,'recent');assert.equal(canvas.style.filter,'brightness(1)');
 click(160,recentY);click(24,sliderY);assert.equal(state().brightnessDraft,10);
 click(296,sliderY);assert.equal(state().brightnessDraft,100);
 click(98,presetY+12);click(160,height-15);assert.equal(state().brightness,25);assert.equal(state().view,'recent');
 vm.runInContext('rotateScreen()',context);
 const rotatedClick=(x,y)=>click(319-x,height-1-y);
 rotatedClick(160,recentY);assert.equal(state().view,'brightness');
 rotatedClick(160,presetY+12);rotatedClick(160,height-15);
 assert.equal(state().brightness,50);assert.equal(state().chosen,'7210');assert.equal(state().north,true);
 rotatedClick(285,14);assert.equal(state().view,'board');
 console.log('PASS: '+height+'px brightness entry, preview/cancel, slider bounds, save and rotated touch');
}
