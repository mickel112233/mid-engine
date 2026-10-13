const VERTEX_SHADER = `
attribute vec3 aPosition;
attribute vec3 aColor;
uniform mat4 uViewProjection;
uniform mat4 uModel;
uniform vec3 uTint;
varying vec3 vColor;
void main() {
  vColor = aColor * uTint;
  gl_Position = uViewProjection * uModel * vec4(aPosition, 1.0);
}`;

const FRAGMENT_SHADER = `
precision mediump float;
varying vec3 vColor;
void main() {
  gl_FragColor = vec4(vColor, 1.0);
}`;

const IDENTITY = new Float32Array([1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1]);
const CUBE_FACES = [
  { corners: [[-1,-1,1],[1,-1,1],[1,1,1],[-1,1,1]], color:[0.78,0.91,0.69] },
  { corners: [[1,-1,-1],[-1,-1,-1],[-1,1,-1],[1,1,-1]], color:[0.46,0.59,0.39] },
  { corners: [[-1,1,1],[1,1,1],[1,1,-1],[-1,1,-1]], color:[0.67,0.82,0.56] },
  { corners: [[-1,-1,-1],[1,-1,-1],[1,-1,1],[-1,-1,1]], color:[0.3,0.42,0.27] },
  { corners: [[1,-1,1],[1,-1,-1],[1,1,-1],[1,1,1]], color:[0.56,0.72,0.45] },
  { corners: [[-1,-1,-1],[-1,-1,1],[-1,1,1],[-1,1,-1]], color:[0.39,0.53,0.32] }
];
const CUBE_VERTICES = new Float32Array(CUBE_FACES.flatMap((face) => {
  const order = [0,1,2,0,2,3];
  return order.flatMap((index) => [...face.corners[index], ...face.color]);
}));

function compileShader(gl, type, source) {
  const shader = gl.createShader(type);
  gl.shaderSource(shader, source);
  gl.compileShader(shader);
  if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
    const message = gl.getShaderInfoLog(shader) || "Unknown shader error";
    gl.deleteShader(shader);
    throw new Error(message);
  }
  return shader;
}
function createProgram(gl) {
  const program = gl.createProgram();
  gl.attachShader(program, compileShader(gl, gl.VERTEX_SHADER, VERTEX_SHADER));
  gl.attachShader(program, compileShader(gl, gl.FRAGMENT_SHADER, FRAGMENT_SHADER));
  gl.linkProgram(program);
  if (!gl.getProgramParameter(program, gl.LINK_STATUS)) throw new Error(gl.getProgramInfoLog(program) || "Could not link WebGL program");
  return program;
}
function normalize(v) {
  const length = Math.hypot(...v) || 1;
  return v.map((n) => n / length);
}
function cross(a,b) { return [a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]]; }
function dot(a,b) { return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]; }
function multiply(a,b) {
  const out = new Float32Array(16);
  for (let column=0; column<4; column++) for (let row=0; row<4; row++) {
    out[column*4+row] = a[row]*b[column*4] + a[4+row]*b[column*4+1] + a[8+row]*b[column*4+2] + a[12+row]*b[column*4+3];
  }
  return out;
}
function perspective(fov, aspect, near, far) {
  const f = 1 / Math.tan(fov/2), nf = 1/(near-far);
  return new Float32Array([f/aspect,0,0,0, 0,f,0,0, 0,0,(far+near)*nf,-1, 0,0,2*far*near*nf,0]);
}
function lookAt(eye,target,up) {
  const z = normalize([eye[0]-target[0],eye[1]-target[1],eye[2]-target[2]]);
  const x = normalize(cross(up,z));
  const y = cross(z,x);
  return new Float32Array([
    x[0],y[0],z[0],0, x[1],y[1],z[1],0, x[2],y[2],z[2],0,
    -dot(x,eye),-dot(y,eye),-dot(z,eye),1
  ]);
}
function translation(x,y,z) { const m=new Float32Array(IDENTITY); m[12]=x;m[13]=y;m[14]=z;return m; }
function scaleMatrix(x,y,z) { const m=new Float32Array(IDENTITY);m[0]=x;m[5]=y;m[10]=z;return m; }
function rotationX(a) { const c=Math.cos(a),s=Math.sin(a);return new Float32Array([1,0,0,0, 0,c,s,0, 0,-s,c,0, 0,0,0,1]); }
function rotationY(a) { const c=Math.cos(a),s=Math.sin(a);return new Float32Array([c,0,-s,0, 0,1,0,0, s,0,c,0, 0,0,0,1]); }
function rotationZ(a) { const c=Math.cos(a),s=Math.sin(a);return new Float32Array([c,s,0,0, -s,c,0,0, 0,0,1,0, 0,0,0,1]); }
function modelMatrix(entity) {
  const t=entity.transform||{}, p=t.position||[0,0,0], r=(t.rotation||[0,0,0]).map((n)=>n*Math.PI/180), s=t.scale||[1,1,1];
  const factor=entity.type==="empty" ? .22 : entity.type==="light" ? .35 : 1;
  return multiply(multiply(multiply(multiply(translation(...p),rotationY(r[1])),rotationX(r[0])),rotationZ(r[2])),scaleMatrix(s[0]*factor,s[1]*factor,s[2]*factor));
}
function createGridVertices() {
  const values=[];
  for(let i=-20;i<=20;i++){
    const major=i%5===0;
    const c=major?[.22,.25,.3]:[.15,.17,.21];
    values.push(i,0,-20,...c,i,0,20,...c,-20,0,i,...c,20,0,i,...c);
  }
  return new Float32Array(values);
}
function bindGeometry(gl,buffer,vertices,positionLocation,colorLocation) {
  gl.bindBuffer(gl.ARRAY_BUFFER,buffer);
  gl.bufferData(gl.ARRAY_BUFFER,vertices,gl.STATIC_DRAW);
  gl.enableVertexAttribArray(positionLocation);
  gl.vertexAttribPointer(positionLocation,3,gl.FLOAT,false,24,0);
  gl.enableVertexAttribArray(colorLocation);
  gl.vertexAttribPointer(colorLocation,3,gl.FLOAT,false,24,12);
}
export function createWebGLRenderer(canvas) {
  const gl=canvas.getContext("webgl",{alpha:false,antialias:true});
  if(!gl) throw new Error("WebGL is unavailable in this browser. Try enabling hardware acceleration or using a supported browser.");
  const program=createProgram(gl);
  const locations={
    position:gl.getAttribLocation(program,"aPosition"),
    color:gl.getAttribLocation(program,"aColor"),
    viewProjection:gl.getUniformLocation(program,"uViewProjection"),
    model:gl.getUniformLocation(program,"uModel"),
    tint:gl.getUniformLocation(program,"uTint")
  };
  const cubeBuffer=gl.createBuffer(),gridBuffer=gl.createBuffer();
  const gridVertices=createGridVertices();
  let yaw=.78,pitch=.58,distance=11,dragging=false,lastX=0,lastY=0;
  gl.enable(gl.DEPTH_TEST);
  gl.enable(gl.CULL_FACE);
  gl.clearColor(.075,.083,.105,1);
  canvas.addEventListener("pointerdown",(event)=>{dragging=true;lastX=event.clientX;lastY=event.clientY;canvas.setPointerCapture?.(event.pointerId);});
  canvas.addEventListener("pointerup",()=>{dragging=false;});
  canvas.addEventListener("pointercancel",()=>{dragging=false;});
  canvas.addEventListener("pointermove",(event)=>{if(!dragging)return;yaw-=(event.clientX-lastX)*.008;pitch=Math.max(-1.25,Math.min(1.25,pitch+(event.clientY-lastY)*.008));lastX=event.clientX;lastY=event.clientY;});
  canvas.addEventListener("wheel",(event)=>{event.preventDefault();distance=Math.max(3,Math.min(30,distance+event.deltaY*.015));},{passive:false});
  function resize() {
    const rect=canvas.getBoundingClientRect(),ratio=Math.min(window.devicePixelRatio||1,2);
    const width=Math.max(1,Math.floor(rect.width*ratio)),height=Math.max(1,Math.floor(rect.height*ratio));
    if(canvas.width!==width||canvas.height!==height){canvas.width=width;canvas.height=height;}
    gl.viewport(0,0,width,height);
  }
  function render(scene,selectedId=null) {
    resize();
    gl.clear(gl.COLOR_BUFFER_BIT|gl.DEPTH_BUFFER_BIT);
    gl.useProgram(program);
    const eye=[distance*Math.cos(pitch)*Math.sin(yaw),distance*Math.sin(pitch),distance*Math.cos(pitch)*Math.cos(yaw)];
    const viewProjection=multiply(perspective(Math.PI/4,canvas.width/canvas.height,.1,150),lookAt(eye,[0,0,0],[0,1,0]));
    gl.uniformMatrix4fv(locations.viewProjection,false,viewProjection);
    gl.uniformMatrix4fv(locations.model,false,IDENTITY);
    gl.uniform3fv(locations.tint,[1,1,1]);
    bindGeometry(gl,gridBuffer,gridVertices,locations.position,locations.color);
    gl.disable(gl.CULL_FACE);
    gl.drawArrays(gl.LINES,0,gridVertices.length/6);
    gl.enable(gl.CULL_FACE);
    for(const entity of (scene?.entities||[])){
      if(entity.enabled===false)continue;
      bindGeometry(gl,cubeBuffer,CUBE_VERTICES,locations.position,locations.color);
      gl.uniformMatrix4fv(locations.model,false,modelMatrix(entity));
      const tint=entity.type==="camera"?[.62,.73,1]:entity.type==="light"?[1,.82,.43]:entity.type==="empty"?[.7,.75,.82]:[1,1,1];
      gl.uniform3fv(locations.tint,entity.id===selectedId?[1.2,1.24,.88]:tint);
      gl.drawArrays(gl.TRIANGLES,0,CUBE_VERTICES.length/6);
    }
  }
  window.addEventListener("resize",()=>{ if(window.__midEngineLastScene) render(window.__midEngineLastScene,window.__midEngineSelectedId); });
  return { render };
}
