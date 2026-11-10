// Renderer, post-processing and quality management. Two scenes are drawn each
// frame: deep space first (its own camera, no depth interaction), then the ship.
import * as THREE from 'three';
import { EffectComposer } from 'three/addons/postprocessing/EffectComposer.js';
import { RenderPass } from 'three/addons/postprocessing/RenderPass.js';
import { UnrealBloomPass } from 'three/addons/postprocessing/UnrealBloomPass.js';
import { OutputPass } from 'three/addons/postprocessing/OutputPass.js';
import { RoomEnvironment } from 'three/addons/environments/RoomEnvironment.js';
import { settings, IS_MOBILE } from '../core/state.js';
import { clamp } from '../core/utils.js';
import { setAnisotropy } from './textures.js';

export const QUALITY = {
  low: { name: 'low', maxDpr: 1, bloom: false, msaa: 0, texSize: 512, particles: 0.4, cube: 512, stars: 1200 },
  medium: { name: 'medium', maxDpr: 1.5, bloom: true, msaa: 0, texSize: 512, particles: 0.7, cube: 1024, stars: 2200 },
  high: { name: 'high', maxDpr: 2, bloom: true, msaa: 4, texSize: 1024, particles: 1, cube: 1024, stars: 3500 },
};

export class Gfx {
  constructor(canvas) {
    this.canvas = canvas;
    this.quality = QUALITY[settings.quality] || QUALITY.medium;
    const renderer = new THREE.WebGLRenderer({
      canvas,
      antialias: !this.quality.bloom,
      powerPreference: 'high-performance',
      stencil: false,
      alpha: false,
    });
    this.renderer = renderer;
    renderer.outputColorSpace = THREE.SRGBColorSpace;
    renderer.toneMapping = THREE.ACESFilmicToneMapping;
    renderer.toneMappingExposure = 1.05;
    renderer.autoClear = false;
    renderer.info.autoReset = false;
    renderer.setClearColor(0x000000, 1);
    setAnisotropy(Math.min(IS_MOBILE ? 4 : 8, renderer.capabilities.getMaxAnisotropy()));

    this.scene = new THREE.Scene();
    this.scene.fog = new THREE.FogExp2(0x05080d, 0.012);
    this.space = new THREE.Scene();
    this.camera = new THREE.PerspectiveCamera(settings.fov, 1, 0.07, 900);
    this.camera.rotation.order = 'YXZ';
    this.spaceCamera = new THREE.PerspectiveCamera(settings.fov, 1, 5, 30000);
    this.scene.add(this.camera);

    const pmrem = new THREE.PMREMGenerator(renderer);
    const envScene = new RoomEnvironment();
    this.scene.environment = pmrem.fromScene(envScene, 0.04).texture;
    this.scene.environmentIntensity = 0.3;
    envScene.traverse((o) => {
      if (o.geometry) o.geometry.dispose();
      if (o.material) o.material.dispose();
    });
    pmrem.dispose();

    this.dprScale = 1;
    this.frameAvg = 16.7;
    this.resTimer = 0;
    this.width = 1;
    this.height = 1;
    this.composer = null;
    this.bloom = null;
    this.bloomBoost = 0;
    this.contextLost = false;
    canvas.addEventListener('webglcontextlost', (e) => {
      e.preventDefault();
      this.contextLost = true;
    });
    canvas.addEventListener('webglcontextrestored', () => {
      this.contextLost = false;
      this.buildComposer();
    });
    this.buildComposer();
    this.resize();
  }

  targetDpr() {
    const base = Math.min(window.devicePixelRatio || 1, this.quality.maxDpr);
    return clamp(base * this.dprScale, 0.5, 2);
  }

  buildComposer() {
    if (this.composer) {
      this.composer.dispose?.();
      this.composer = null;
      this.bloom = null;
    }
    if (!this.quality.bloom) return;
    const r = this.renderer;
    const size = r.getDrawingBufferSize(new THREE.Vector2());
    const target = new THREE.WebGLRenderTarget(Math.max(2, size.x), Math.max(2, size.y), {
      type: THREE.HalfFloatType,
      samples: this.quality.msaa,
    });
    const composer = new EffectComposer(r, target);
    const spacePass = new RenderPass(this.space, this.spaceCamera);
    spacePass.clear = true;
    const mainPass = new RenderPass(this.scene, this.camera);
    mainPass.clear = false;
    mainPass.clearDepth = true;
    const bloom = new UnrealBloomPass(new THREE.Vector2(size.x, size.y), 0.55, 0.62, 1.0);
    composer.addPass(spacePass);
    composer.addPass(mainPass);
    composer.addPass(bloom);
    composer.addPass(new OutputPass());
    this.composer = composer;
    this.bloom = bloom;
  }

  setQuality(name) {
    const q = QUALITY[name] || QUALITY.medium;
    this.quality = q;
    this.dprScale = 1;
    this.buildComposer();
    this.resize();
  }

  resize() {
    const w = Math.max(1, this.canvas.clientWidth || window.innerWidth);
    const h = Math.max(1, this.canvas.clientHeight || window.innerHeight);
    this.width = w;
    this.height = h;
    const dpr = this.targetDpr();
    this.renderer.setPixelRatio(dpr);
    this.renderer.setSize(w, h, false);
    if (this.composer) {
      this.composer.setPixelRatio(dpr);
      this.composer.setSize(w, h);
    }
    this.camera.aspect = w / h;
    this.spaceCamera.aspect = w / h;
    this.applyFov();
  }

  // Keep a sensible horizontal view on tall phone screens.
  applyFov(extra = 0) {
    let fov = settings.fov + extra;
    const aspect = this.camera.aspect;
    if (aspect < 1) {
      const h = 2 * Math.atan(Math.tan((fov * Math.PI) / 360) * 1.0);
      fov = clamp((h * 180) / Math.PI + (1 - aspect) * 22, 50, 105);
    }
    this.camera.fov = fov;
    this.spaceCamera.fov = fov;
    this.camera.updateProjectionMatrix();
    this.spaceCamera.updateProjectionMatrix();
  }

  // Lower the internal resolution when frames run long, raise it when there is headroom.
  adapt(dtMs) {
    this.frameAvg += (dtMs - this.frameAvg) * 0.05;
    if (!settings.dynamicRes) return;
    this.resTimer += dtMs;
    if (this.resTimer < 1500) return;
    this.resTimer = 0;
    let next = this.dprScale;
    if (this.frameAvg > 26 && this.dprScale > 0.55) next = Math.max(0.55, this.dprScale - 0.1);
    else if (this.frameAvg < 17.5 && this.dprScale < 1) next = Math.min(1, this.dprScale + 0.05);
    if (next !== this.dprScale) {
      this.dprScale = next;
      this.resize();
    }
  }

  render() {
    if (this.contextLost) return;
    const r = this.renderer;
    r.info.reset();
    r.toneMappingExposure = 1.05 * settings.brightness;
    this.spaceCamera.quaternion.copy(this.camera.getWorldQuaternion(_q));
    if (this.composer) {
      this.bloom.strength = 0.55 + this.bloomBoost;
      this.composer.render();
    } else {
      r.setRenderTarget(null);
      r.clear(true, true, false);
      r.render(this.space, this.spaceCamera);
      r.clearDepth();
      r.render(this.scene, this.camera);
    }
  }
}

const _q = new THREE.Quaternion();
