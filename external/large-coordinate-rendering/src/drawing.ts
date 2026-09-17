import * as THREE from 'three';
import { RTE_SPLIT_TRANSLATION_FLAG, RelativeToEyeUtil } from './relativeToEye';
import {
  createCirclePoints,
  createLineLoopObject,
  createLineObject,
  createPolylinePoints,
  createRectangleLoopPoints,
  translatePoints,
} from './utils';

export type RenderingMode = 'raw' | 'rebased' | 'relative-to-eye';

const WORLD_BASE_POINT = /* @__PURE__ */ new THREE.Vector3(4_000_000_000, 4_000_000_000, 0);
const DETAIL_CENTER_OFFSET = /* @__PURE__ */ new THREE.Vector3(1_536, -1_024, 0);

type SampleShape = {
  color: number;
  points: THREE.Vector3[];
  closed?: boolean;
};

export class Drawing {
  private _mode: RenderingMode;
  private _rootGroup: THREE.Group;

  constructor(mode: RenderingMode = 'relative-to-eye') {
    this._mode = mode;
    this._rootGroup = new THREE.Group();
    this.buildDraw();
  }

  get rootGroup() {
    return this._rootGroup;
  }

  get mode() {
    return this._mode;
  }

  set mode(value: RenderingMode) {
    if (this._mode === value) return;

    this._mode = value;
    this.clear();
    this.buildDraw();
  }

  /**
   * Returns world base point
   */
  get worldBasePoint() {
    return WORLD_BASE_POINT.clone();
  }

  /**
   * Returns actual world position of the detail cluster
   */
  get detailCenterWorld() {
    return WORLD_BASE_POINT.clone().add(DETAIL_CENTER_OFFSET);
  }

  /**
   * Whether RTE mode is enabled
   */
  get usesRelativeToEye() {
    return this._mode === 'relative-to-eye';
  }

  /**
   * Remove all objects and dispose resources
   */
  clear() {
    const group = this._rootGroup;

    while (group.children.length > 0) {
      const child = group.children[0];
      group.remove(child);
      this.disposeObject(child);
    }
  }

  private buildDraw() {
    this._rootGroup.position.set(0, 0, 0);

    if (this._mode === 'raw') {
      this.buildRawWorldGeometry();
      return;
    }

    this.buildRebasedGeometry(this._mode === 'relative-to-eye');
  }

  /**
   * RAW MODE:
   * Geometry is placed directly in world coordinates (worst precision)
   */
  private buildRawWorldGeometry() {
    const worldOffset = this.detailCenterWorld;

    this.createSampleShapes().forEach((shape) => {
      const worldPoints = translatePoints(shape.points, worldOffset);
      this._rootGroup.add(this.createRenderable(shape, worldPoints));
    });
  }

  /**
   * REBASED MODE:
   * Geometry is small, but modelMatrix contains large values
   * → still suffers from precision issues
   */
  private buildRebasedGeometry(useRelativeToEye: boolean) {
    /**
     * Root carries huge world translation (1e9)
     */
    this._rootGroup.position.copy(WORLD_BASE_POINT);

    const detailGroup = new THREE.Group();

    /**
     * Second-level offset (1e7)
     * This is the key precision killer
     */
    detailGroup.position.copy(DETAIL_CENTER_OFFSET);

    this.createSampleShapes().forEach((shape) => {
      const renderable = this.createRenderable(shape, shape.points);

      /**
       * Enable Relative-To-Eye (GPU precision fix)
       */
      if (useRelativeToEye) {
        renderable.userData[RTE_SPLIT_TRANSLATION_FLAG] = true;
        RelativeToEyeUtil.enableForObject(renderable);
      }

      detailGroup.add(renderable);
    });

    this._rootGroup.add(detailGroup);
  }

  /**
   * Create renderable object (Line or LineLoop)
   */
  private createRenderable(shape: SampleShape, points: THREE.Vector3[]) {
    return shape.closed
      ? createLineLoopObject(points, shape.color)
      : createLineObject(points, shape.color);
  }

  /**
   * Create test shapes designed to expose precision issues
   */
  private createSampleShapes(): SampleShape[] {
    const shapes: SampleShape[] = [];

    /**
     * Large circle (reference shape, usually stable)
     */
    shapes.push({
      color: 0x6ee7f9,
      closed: true,
      points: createCirclePoints(0, 0, 1_280, 256),
    });

    /**
     * Extremely small rectangle (high precision sensitivity)
     */
    shapes.push({
      color: 0xf6d860,
      closed: true,
      points: createCirclePoints(0, 0, 224, 96),
    });

    /**
     * Thin cross lines (easy to jitter)
     */
    shapes.push({
      color: 0xffffff,
      points: createPolylinePoints([
        [-1_920, 0],
        [1_920, 0],
      ]),
    });

    shapes.push({
      color: 0xffffff,
      points: createPolylinePoints([
        [0, -1_920],
        [0, 1_920],
      ]),
    });

    shapes.push({
      color: 0x7cf29a,
      points: createPolylinePoints([
        [-384, -416],
        [-320, -416],
        [-320, -352],
        [-256, -352],
        [-256, -288],
        [-192, -288],
        [-192, -224],
        [-128, -224],
        [-128, -160],
        [-64, -160],
        [-64, -96],
        [0, -96],
        [0, -32],
        [64, -32],
        [64, 32],
        [128, 32],
        [128, 96],
        [192, 96],
        [192, 160],
        [256, 160],
      ]),
    });

    const microFrameOffsets = [
      new THREE.Vector3(-288, 224, 0),
      new THREE.Vector3(-96, 224, 0),
      new THREE.Vector3(96, 224, 0),
      new THREE.Vector3(288, 224, 0),
    ];
    microFrameOffsets.forEach((offset) => {
      shapes.push({
        color: 0xff8c69,
        closed: true,
        points: translatePoints(createRectangleLoopPoints(-32, -32, 64, 64), offset),
      });
    });

    shapes.push({
      color: 0xc084fc,
      closed: true,
      points: createRectangleLoopPoints(-540, -1_420, 1_080, 220),
    });

    return shapes;
  }

  /**
   * Dispose geometries and materials
   */
  private disposeObject(object: THREE.Object3D) {
    object.traverse((child) => {
      const renderable = child as THREE.Mesh;

      if (renderable.geometry) {
        renderable.geometry.dispose();
      }

      if (renderable.material) {
        if (Array.isArray(renderable.material)) {
          renderable.material.forEach((m) => m.dispose());
        } else {
          renderable.material.dispose();
        }
      }
    });
  }
}
