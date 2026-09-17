import * as THREE from 'three';
import './style.css';
import { Drawing, type RenderingMode } from './drawing';
import { View2d } from './view';

const drawing = new Drawing('relative-to-eye');
const canvas = document.getElementById('cad-viewer-canvas') as HTMLCanvasElement;
const view2d = new View2d(canvas, drawing);

const modeDescriptions: Record<RenderingMode, string> = {
  raw: 'Directly write large coordinates into BufferGeometry. Precision is lost when vertices are uploaded to Float32Array, causing small details to collapse.',
  rebased:
    'First subtract the global base point, then put the root node back to large world coordinates. Geometry data is safe, but the world base point is still large.',
  'relative-to-eye':
    'Based on rebase, modify the vertex shader to use camera-relative coordinate calculations. This is the relative-to-eye shader patch used in the current branch.',
};

const modeButtons = Array.from(document.querySelectorAll<HTMLButtonElement>('[data-mode]'));
const labelMode = document.getElementById('mode-name')!;
const labelDescription = document.getElementById('mode-description')!;
const labelBasePoint = document.getElementById('base-point')!;
const labelFeatureCenter = document.getElementById('feature-center')!;
const labelX = document.getElementById('mouse-x')!;
const labelY = document.getElementById('mouse-y')!;

function formatVector(vector: THREE.Vector3) {
  return `(${vector.x.toLocaleString()}, ${vector.y.toLocaleString()}, ${vector.z.toLocaleString()})`;
}

function setMode(mode: RenderingMode) {
  drawing.mode = mode;
  labelMode.textContent = mode;
  labelDescription.textContent = modeDescriptions[mode];

  modeButtons.forEach((button) => {
    button.dataset.active = button.dataset.mode === mode ? 'true' : 'false';
  });

  view2d.zoomToFit();
}

modeButtons.forEach((button) => {
  button.addEventListener('click', () => {
    setMode(button.dataset.mode as RenderingMode);
  });
});

labelBasePoint.textContent = formatVector(drawing.worldBasePoint);
labelFeatureCenter.textContent = formatVector(drawing.detailCenterWorld);
setTimeout(() => {
  setMode(drawing.mode);
}, 0);

view2d.canvas.addEventListener('mousemove', (event) => {
  const rect = view2d.canvas.getBoundingClientRect();
  const cx = event.clientX - rect.left;
  const cy = event.clientY - rect.top;
  const world = view2d.cwcs2Wcs(new THREE.Vector2(cx, cy));

  labelX.textContent = `X: ${world.x.toFixed(2)}`;
  labelY.textContent = `Y: ${world.y.toFixed(2)}`;
});
