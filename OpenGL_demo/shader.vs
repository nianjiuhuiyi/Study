// ����������ɫ��ʹ���ܹ����ܶ�������Ϊһ���������ԣ��������괫��Ƭ����ɫ��
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
layout(location = 2) in vec2 aTexCoord;  // ���0��1��2�ʹ��������ö��������Ƕ�Ӧ��

out vec3 ourColor;
out vec2 TexCoord;

void main() {
	gl_Position = vec4(aPos, 1.0);
	ourColor = aColor;
	TexCoord = aTexCoord;
}


