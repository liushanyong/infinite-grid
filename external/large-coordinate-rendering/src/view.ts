import * as THREE from 'three';
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls.js';
import { Camera } from './camera';
import { RelativeToEyeUtil } from './relativeToEye';
import { Scene } from './scene';
import type { Drawing } from './drawing';

export class View2d {
  protected _frustum = 400;
  protected _canvas: HTMLCanvasElement;
  private _width: number;
  private _height: number;
  private _camera: Camera;
  private _cameraControls: OrbitControls;
  private _renderer: THREE.WebGLRenderer;
  private _scene: Scene;
  private _drawing: Drawing;
  private _curPos: THREE.Vector2;
  private _curScreenPos: THREE.Vector2;

  constructor(canvas: HTMLCanvasElement, drawing: Drawing) {
    const renderer = new THREE.WebGLRenderer({
      antialias: true,
      alpha: true,
      canvas,
    });
    renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
    renderer.setSize(window.innerWidth, window.innerHeight);
    this._renderer = renderer;

    this._canvas = renderer.domElement;
    this._canvas.addEventListener('mousemove', (event) => this.onMouseMove(event));

    const rect = this._canvas.getBoundingClientRect();
    this._width = rect.width;
    this._height = rect.height;

    const camera = this.createCamera();
    this._camera = new Camera(camera);
    this._cameraControls = this.createCameraControls();

    this._drawing = drawing;
    this._scene = new Scene();
    this._scene.add(drawing.rootGroup);

    this._curPos = new THREE.Vector2();
    this._curScreenPos = new THREE.Vector2();

    window.addEventListener('resize', this.onWindowResize.bind(this));
    this.animate();
  }

  get canvas() {
    return this._canvas;
  }

  get camera() {
    return this._camera;
  }

  get scene() {
    return this._scene;
  }

  animate() {
    requestAnimationFrame(this.animate.bind(this));
    if (this._drawing.usesRelativeToEye) {
      RelativeToEyeUtil.prepareScene(this._scene.internalScene);
    }
    this._renderer.render(this._scene.internalScene, this._camera.internalCamera);
  }

  clear() {
    this._scene.clear();
  }

  zoomTo(box: THREE.Box3, margin: number = 1.1) {
    const size = new THREE.Vector3();
    box.getSize(size);

    const center = new THREE.Vector3();
    box.getCenter(center);

    this._camera.position.set(center.x, center.y, this._camera.position.z);
    this._camera.lookAt(center);
    this._camera.setRotationFromEuler(new THREE.Euler(0, 0, 0));

    const width = Math.max(size.x * margin, 1);
    const height = Math.max(size.y * margin, 1);
    const widthRatio = this._width / width;
    const heightRatio = this._height / height;
    this._camera.zoom = Math.min(widthRatio, heightRatio);

    this._cameraControls.target.copy(center);
    this.updateCameraFrustum();
  }

  zoomToFit() {
    this.zoomTo(this._scene.box);
  }

  cwcs2Wcs(point: THREE.Vector2Like): THREE.Vector2 {
    return this._camera.cwcs2Wcs(point, this._width, this._height);
  }

  wcs2Cwcs(point: THREE.Vector2Like): THREE.Vector2 {
    return this._camera.wcs2Cwcs(point, this._width, this._height);
  }

  protected updateCameraFrustum(width?: number, height?: number) {
    const aspect = (width ?? this._width) / (height ?? this._height);
    this._camera.left = -aspect * this._frustum;
    this._camera.right = aspect * this._frustum;
    this._camera.top = this._frustum;
    this._camera.bottom = -this._frustum;
    this._camera.updateProjectionMatrix();
    this._cameraControls.update();
  }

  protected onWindowResize() {
    this._width = window.innerWidth;
    this._height = window.innerHeight;
    this._renderer.setSize(this._width, this._height);
    this.updateCameraFrustum(this._width, this._height);
  }

  private onMouseMove(event: MouseEvent) {
    const rect = this._canvas.getBoundingClientRect();
    this._curScreenPos = new THREE.Vector2(event.clientX - rect.left, event.clientY - rect.top);
    const wcsPos = this.cwcs2Wcs(this._curScreenPos);
    this._curPos.copy(wcsPos);
  }

  private createCamera() {
    const cameraLen = 500;
    const camera = new THREE.OrthographicCamera(
      -this._width / 2,
      this._width / 2,
      this._height / 2,
      -this._height / 2,
      0.1,
      1000
    );
    camera.position.set(0, 0, cameraLen);
    camera.up.set(0, 1, 0);
    camera.updateProjectionMatrix();
    return camera;
  }

  private createCameraControls() {
    const cameraControls = new OrbitControls(
      this._camera.internalCamera,
      this._renderer.domElement
    );
    cameraControls.enableDamping = false;
    cameraControls.autoRotate = false;
    cameraControls.enableRotate = false;
    cameraControls.zoomSpeed = 5;
    cameraControls.mouseButtons = {
      MIDDLE: THREE.MOUSE.PAN,
    };
    cameraControls.update();
    return cameraControls;
  }
}
