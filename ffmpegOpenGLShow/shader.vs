// 调整顶点着色器使其能够接受顶点坐标为一个顶点属性，并把坐标传给片段着色器
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
layout(location = 2) in vec2 aTexCoord;  // 这个0、1、2和代码中设置顶点属性是对应的

out vec3 ourColor;
out vec2 TexCoord;

void main() {
	gl_Position = vec4(aPos, 1.0);
	ourColor = aColor;
	TexCoord = aTexCoord;
}


