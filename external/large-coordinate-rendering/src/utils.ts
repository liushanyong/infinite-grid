import * as THREE from 'three';

export type Point2Tuple = readonly [number, number];

export function createCirclePoints(
  centerX: number,
  centerY: number,
  radius: number,
  segments: number = 128
): THREE.Vector3[] {
  const points: THREE.Vector3[] = [];

  for (let i = 0; i < segments; i += 1) {
    const theta = (i / segments) * Math.PI * 2;
    points.push(
      new THREE.Vector3(centerX + Math.cos(theta) * radius, centerY + Math.sin(theta) * radius, 0)
    );
  }

  return points;
}

export function createPolylinePoints(points: Point2Tuple[]): THREE.Vector3[] {
  return points.map(([x, y]) => new THREE.Vector3(x, y, 0));
}

export function createRectangleLoopPoints(
  minX: number,
  minY: number,
  width: number,
  height: number
): THREE.Vector3[] {
  return createPolylinePoints([
    [minX, minY],
    [minX + width, minY],
    [minX + width, minY + height],
    [minX, minY + height],
  ]);
}

export function translatePoints(
  points: THREE.Vector3[],
  offset: THREE.Vector3Like
): THREE.Vector3[] {
  return points.map(
    (point) => new THREE.Vector3(point.x + offset.x, point.y + offset.y, point.z + offset.z)
  );
}

export function createLineObject(points: THREE.Vector3[], color: number = 0xffffff): THREE.Line {
  const geometry = new THREE.BufferGeometry().setFromPoints(points);
  const material = new THREE.LineBasicMaterial({ color });
  return new THREE.Line(geometry, material);
}

export function createLineLoopObject(
  points: THREE.Vector3[],
  color: number = 0xffffff
): THREE.LineLoop {
  const geometry = new THREE.BufferGeometry().setFromPoints(points);
  const material = new THREE.LineBasicMaterial({ color });
  return new THREE.LineLoop(geometry, material);
}
