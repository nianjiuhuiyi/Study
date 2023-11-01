#include "game.h"
#include "resource_manager.h"
#include "sprite_renderer.h"
#include <iostream>

// Game-related State data
SpriteRenderer  *Renderer;

// 玩家操控的挡板
GameObject      *Player;


Game::Game(GLuint width, GLuint height)
	: State(GAME_ACTIVE), Keys(), Width(width), Height(height) {

}

Game::~Game() {
	delete Renderer;
	delete Player;
}

void Game::Init() {
	// Load shaders
	ResourceManager::LoadShader("E:\\VS_project\\Study\\LearnOpenGL\\shaders\\sprite.vs", "E:\\VS_project\\Study\\LearnOpenGL\\shaders\\sprite.frag", nullptr, "sprite");
	// Configure shaders
	glm::mat4 projection = glm::ortho(0.0f, static_cast<GLfloat>(this->Width), static_cast<GLfloat>(this->Height), 0.0f, -1.0f, 1.0f);
	ResourceManager::GetShader("sprite").Use().SetInteger("image", 0);
	ResourceManager::GetShader("sprite").SetMatrix4("projection", projection);
	// Set render-specific controls
	Shader shader = ResourceManager::GetShader("sprite");
	Renderer = new SpriteRenderer(shader);


	// Load textures
	std::string PicDir = "C:\\Users\\Administrator\\Downloads\\";
	ResourceManager::LoadTexture((PicDir + "background.jpg").c_str(), GL_FALSE, "background");
	ResourceManager::LoadTexture((PicDir + "awesomeface.png").c_str(), GL_TRUE, "face");
	ResourceManager::LoadTexture((PicDir + "block.png").c_str(), GL_FALSE, "block");
	ResourceManager::LoadTexture((PicDir + "block_solid.png").c_str(), GL_FALSE, "block_solid");
	ResourceManager::LoadTexture((PicDir + "paddle.png").c_str(), GL_TRUE, "paddle");

	// 加载关卡
	std::string LvlDir = "E:\\VS_project\\Study\\LearnOpenGL\\levels\\";
	GameLevel one; one.Load((LvlDir + "one.lvl").c_str(), this->Width, this->Height / 2);
	GameLevel two; two.Load((LvlDir + "tow.lvl").c_str(), this->Width, this->Height / 2);
	GameLevel three; three.Load((LvlDir + "three.lvl").c_str(), this->Width, this->Height / 2);
	GameLevel four; four.Load((LvlDir + "four.lvl").c_str(), this->Width, this->Height / 2);
	this->Levels.push_back(one);
	this->Levels.push_back(two);
	this->Levels.push_back(three);
	this->Levels.push_back(four);
	this->Level = 0;   // 默认初始化在第一关


	// 设置玩家操控挡板的属性
	glm::vec2 playerPos = glm::vec2(this->Width / 2.0f - PLAYER_SIZE.x / 2.0f, this->Height - PLAYER_SIZE.y);
	Player = new GameObject(playerPos, PLAYER_SIZE, ResourceManager::GetTexture("paddle"));


	
}

void Game::Update(GLfloat dt) {

}


void Game::ProcessInput(GLfloat dt) {
	if (this->State == GAME_ACTIVE) {
		float velocity = PLAYER_VELOCITY * dt;
		// 移动挡板
		if (this->Keys[GLFW_KEY_A]) {
			if (Player->Position.x >= 0)
				Player->Position.x -= velocity;
		}
		if (this->Keys[GLFW_KEY_D]) {
			if (Player->Position.x <= this->Width - Player->Size.x)
				Player->Position.x += velocity;
		}
	}
}

void Game::Render() {
	// 一开始的笑脸
	/*Texture2D texture = ResourceManager::GetTexture("face");
	Renderer->DrawSprite(texture, glm::vec2(200, 200), glm::vec2(300, 400), 45.0f, glm::vec3(0.0f, 1.0f, 0.0f));*/

	if (this->State == GAME_ACTIVE) {
		// 绘制背景
		Texture2D texture = ResourceManager::GetTexture("background");
		Renderer->DrawSprite(texture, glm::vec2(0.0f, 0.0f), glm::vec2(this->Width, this->Height), 0.0f);
		// 绘制关卡
		this->Levels[this->Level].Draw(*Renderer);
		// 绘制玩家操作挡板
		Player->Draw(*Renderer);
	}

}