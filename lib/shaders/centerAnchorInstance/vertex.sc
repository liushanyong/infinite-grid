$input a_normal, a_position, i_data0, i_data1, i_data2, i_data3  
$output v_normal, v_color, v_viewDepth  
#include "bgfx_shader.sh"  
uniform mat4 uView;  
uniform mat4 projection;  
uniform vec4 uLogDepth;  
void main()  
{  
    // Explicit column-major 3x4 transform: translation lives in .w of each
    // transform column.  This avoids mtxFromCols transpose differences.
    vec3 worldPosRebased =
        a_position.x * i_data0.xyz +
        a_position.y * i_data1.xyz +
        a_position.z * i_data2.xyz +
        vec3(i_data0.w, i_data1.w, i_data2.w);
    vec4 worldPosition = vec4(worldPosRebased, 1.0);  
    vec4 viewPosition = mul(uView, worldPosition);  
    vec3 transformedNormal =
        a_normal.x * i_data0.xyz +
        a_normal.y * i_data1.xyz +
        a_normal.z * i_data2.xyz;  
    v_normal = normalize(transformedNormal);  
    v_color = i_data3;
    v_viewDepth = -viewPosition.z;  
    gl_Position = mul(projection, viewPosition);  
}  
