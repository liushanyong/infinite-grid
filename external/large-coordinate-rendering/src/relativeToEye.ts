import * as THREE from 'three';

const RTE_RELATIVE_WORLD_POSITION = 'uDemoRelativeWorldPosition';
const RTE_VIEW_ROTATION_MATRIX = 'uDemoViewRotationMatrix';

const RTE_MATERIAL_FLAG = '__demoRteMaterialEnabled';
const RTE_OBJECT_FLAG = '__demoRteObjectEnabled';
const RTE_SHADER_REF = '__demoRteShader';
const RTE_PROGRAM_KEY = 'demo:rte:v2';

/**
 * Flag to indicate this object uses RTE translation
 */
export const RTE_SPLIT_TRANSLATION_FLAG = '__demoUseSplitTranslation';

type RteShader = {
  uniforms: Record<string, { value: unknown }> & {
    [RTE_RELATIVE_WORLD_POSITION]: { value: THREE.Vector3 };
    [RTE_VIEW_ROTATION_MATRIX]: { value: THREE.Matrix4 };
  };
};

const _cameraWorldPosition = new THREE.Vector3();
const _objectWorldPosition = new THREE.Vector3();
const _relativeWorldPosition = new THREE.Vector3();
const _viewRotationMatrix = new THREE.Matrix4();

export class RelativeToEyeUtil {
  /**
   * Enable RTE for entire scene
   */
  static prepareScene(scene: THREE.Object3D) {
    scene.traverse((object) => {
      if (!('material' in object) || object.material == null) return;

      this.enableForObject(
        object as THREE.Object3D & {
          material: THREE.Material | THREE.Material[];
          userData: Record<string, unknown>;
        }
      );
    });
  }

  /**
   * Enable RTE for a specific object
   */
  static enableForObject(
    object: THREE.Object3D & {
      material?: THREE.Material | THREE.Material[];
      userData: Record<string, unknown>;
    }
  ) {
    if (object.userData[RTE_OBJECT_FLAG]) return;

    if (object.material) {
      this.ensureMaterialPatched(object.material);
    }

    const prevOnBeforeRender = object.onBeforeRender;

    object.onBeforeRender = function (renderer, scene, camera, geometry, material, group) {
      prevOnBeforeRender?.call(this, renderer, scene, camera, geometry, material, group);

      RelativeToEyeUtil.ensureMaterialPatched(material);
      RelativeToEyeUtil.updateUniforms(this as THREE.Object3D, material, camera);
    };

    object.userData[RTE_OBJECT_FLAG] = true;
  }

  /**
   * Patch material shader
   */
  private static ensureMaterialPatched(material: THREE.Material | THREE.Material[]) {
    if (Array.isArray(material)) {
      material.forEach((m) => this.ensureMaterialPatched(m));
      return;
    }

    if (material.userData[RTE_MATERIAL_FLAG] === RTE_PROGRAM_KEY) return;

    const prevCompile = material.onBeforeCompile.bind(material);
    const prevKey = material.customProgramCacheKey?.bind(material);

    material.onBeforeCompile = (shader, renderer) => {
      prevCompile(shader, renderer);
      this.ensureUniforms(shader);
      shader.vertexShader = this.patchVertexShader(shader.vertexShader);
      material.userData[RTE_SHADER_REF] = shader;
    };

    material.customProgramCacheKey = () => {
      return `${prevKey?.() ?? ''}|${RTE_PROGRAM_KEY}`;
    };

    material.userData[RTE_MATERIAL_FLAG] = RTE_PROGRAM_KEY;
    material.needsUpdate = true;
  }

  /**
   * Update uniforms per frame (CPU double precision)
   */
  private static updateUniforms(
    object: THREE.Object3D,
    material: THREE.Material | THREE.Material[],
    camera: THREE.Camera
  ) {
    if (Array.isArray(material)) {
      material.forEach((m) => this.updateUniforms(object, m, camera));
      return;
    }

    const shader = material.userData[RTE_SHADER_REF] as RteShader | undefined;
    if (!shader) return;

    this.ensureUniforms(shader);

    camera.getWorldPosition(_cameraWorldPosition);
    _objectWorldPosition.setFromMatrixPosition(object.matrixWorld);

    /**
     * Compute relative translation on CPU (high precision)
     */
    _relativeWorldPosition.copy(_objectWorldPosition).sub(_cameraWorldPosition);

    /**
     * Extract view rotation only (remove translation)
     */
    _viewRotationMatrix.copy(camera.matrixWorldInverse);
    _viewRotationMatrix.setPosition(0, 0, 0);

    shader.uniforms[RTE_RELATIVE_WORLD_POSITION].value.copy(_relativeWorldPosition);
    shader.uniforms[RTE_VIEW_ROTATION_MATRIX].value.copy(_viewRotationMatrix);
  }

  /**
   * Ensure uniforms exist
   */
  private static ensureUniforms(shader: { uniforms: Record<string, { value: unknown }> }) {
    const uniforms = shader.uniforms as RteShader['uniforms'];

    uniforms[RTE_RELATIVE_WORLD_POSITION] ??= { value: new THREE.Vector3() };
    uniforms[RTE_VIEW_ROTATION_MATRIX] ??= { value: new THREE.Matrix4() };
  }

  /**
   * 🔥 CORE FIX:
   * Remove large translation from modelMatrix on GPU
   */
  private static patchVertexShader(vertexShader: string) {
    const src = this.injectUniforms(vertexShader);

    if (!src.includes('#include <project_vertex>')) return src;

    return src.replace(
      '#include <project_vertex>',
      /* glsl */ `
        vec3 localPosition = transformed;

        #ifdef USE_BATCHING
          localPosition = ( batchingMatrix * vec4(localPosition, 1.0) ).xyz;
        #endif

        #ifdef USE_INSTANCING
          localPosition = ( instanceMatrix * vec4(localPosition, 1.0) ).xyz;
        #endif

        /**
         * Remove translation from modelMatrix
         */
        mat4 modelNoTranslation = modelMatrix;
        modelNoTranslation[3].xyz = vec3(0.0);

        vec3 worldNoTranslation =
          ( modelNoTranslation * vec4(localPosition, 1.0) ).xyz;

        /**
         * Add relative translation (small numbers only)
         */
        vec3 relativePosition =
          worldNoTranslation + ${RTE_RELATIVE_WORLD_POSITION};

        /**
         * Apply camera rotation only
         */
        vec4 mvPosition =
          ${RTE_VIEW_ROTATION_MATRIX} * vec4(relativePosition, 1.0);

        gl_Position = projectionMatrix * mvPosition;
      `
    );
  }

  /**
   * Inject uniforms into shader
   */
  private static injectUniforms(vertexShader: string) {
    if (
      vertexShader.includes(`uniform vec3 ${RTE_RELATIVE_WORLD_POSITION};`) &&
      vertexShader.includes(`uniform mat4 ${RTE_VIEW_ROTATION_MATRIX};`)
    ) {
      return vertexShader;
    }

    return /* glsl */ `
uniform vec3 ${RTE_RELATIVE_WORLD_POSITION};
uniform mat4 ${RTE_VIEW_ROTATION_MATRIX};
${vertexShader}`;
  }
}
