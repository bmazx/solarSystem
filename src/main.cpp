#include <ctime>
#include <string>
#include <unistd.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstdio>
#include <cmath>
#include <cstdint>
#include <vector>
#include <chrono>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include "ogls.h"


#define COLOR_FG 1.0, 1.0, 1.0
#define COLOR_BG 0.0, 0.0, 0.0

#define SUN_COLOR 1.0, 0.92, 0
#define MERCURY_COLOR 0.64, 0.65, 0.68
#define VENUS_COLOR 0.90, 0.76, 0.57
#define EARTH_COLOR 0.26, 0.58, 0.94
#define MARS_COLOR 0.96, 0.28, 0.24
#define JUPITER_COLOR 0.85, 0.56, 0.16
#define SATURN_COLOR 0.59, 0.49, 0.36
#define URANUS_COLOR 0.0, 0.53, 0.66
#define NEPTUNE_COLOR 0.06, 0.20, 0.53
#define PLUTO_COLOR 0.91, 0.91, 0.91
#define TRAIL_LINE_COLOR 0.43, 0.43, 0.43

#define G_CONSTANT 6.6743e-11 /* G Constant of attraction */
#define AU 1.496e+11 /* 1 AU in meters */
#define SCREEN_SCALE static_cast<float>(2.67379679e-9) /* 400/1.496e+11 (aka 300px / 1AU) */

#define PI (22.0f/7.0f) /* 3.1415... */


static const uint32_t s_MaxVertices = 1024;
static const uint32_t s_MaxIndices = s_MaxVertices * 4;
static const uint32_t s_MaxTrailVertices = UINT16_MAX;

class Timer
{
private:
	std::chrono::time_point<std::chrono::high_resolution_clock> m_Time;

public:
	void start() { m_Time = std::chrono::high_resolution_clock::now(); }
	void reset() { m_Time = std::chrono::high_resolution_clock::now(); }
	float elapsed() { return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - m_Time).count() * 0.001f * 0.001f * 0.001f; }
	float elapsedms() { return elapsed() * 1000.0f; }
};

struct Vertex
{
	OglsVec2 pos;
	OglsVec3 color;
};

struct BatchGroup
{
	OglsVertexBuffer* vertexBuffer;
	OglsIndexBuffer* indexBuffer;
	OglsVertexArray* vertexArray;
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
};

const char* vertexShaderSource = R"(
#version 330 core

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec3 aColor;

out vec3 fragColor;

uniform mat4 u_Camera;

void main()
{
    gl_Position = u_Camera * vec4(aPos, 0.0, 1.0);
	fragColor = aColor;
}
)";

const char* fragmentShaderSource = R"(
#version 330 core

in vec3 fragColor;

out vec4 outColor;

void main()
{
    outColor = vec4(fragColor, 1.0f);
}
)";

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

float radians(float deg)
{
	return (deg * PI * 0.005555f);
}

void cameraMovement(GLFWwindow* window, float* x, float* y, float dt)
{
	float speed = 1000.0f;

	if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
	{
		speed *= 10.0f;
	}

	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
	{
		*x -= speed * dt;
	}
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
	{
		*x += speed * dt;
	}
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
	{
		*y -= speed * dt;
	}
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
	{
		*y += speed * dt;
	}
}

void cameraScale(GLFWwindow* window, float* scale, float dt)
{
	float speed = 10.0f;

	if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
	{
		speed *= 10.0f;
	}

	if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS)
	{
		*scale += speed * dt;
	}
	if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS)
	{
		*scale -= speed * dt;
	}

	if (*scale < 0.5f)
		*scale = 0.5f;
}

void drawPoly(BatchGroup* batch, OglsVec2 pos, OglsVec3 color, float radius, uint32_t nSides)
{
	batch->vertices.clear();
	batch->indices.clear();

	batch->vertices.push_back({ pos, color });

	float twoPi = (float)(2 * PI);
	float angle = twoPi / (float)nSides;

	for (uint32_t i = 0; i < nSides; i++)
	{
		float circlex = std::cos(i * angle);
		float circley = std::sin(i * angle);

		float posx = pos.x + (radius * circlex);
		float posy = pos.y + (radius * circley);

		batch->vertices.push_back({{ posx, posy}, color });
		batch->indices.push_back(0);
		batch->indices.push_back(i + 1);
		batch->indices.push_back(i + 2);
	}

	// connect the last vertex off the last triangle to the first vertex on the circle
	batch->indices.back() = batch->indices[1];

	ogls::bindVertexBufferSubData(batch->vertexBuffer, batch->vertices.size() * sizeof(Vertex), 0, (float*)batch->vertices.data());
	ogls::bindIndexBufferSubData(batch->indexBuffer, batch->indices.size() * sizeof(uint32_t), 0, batch->indices.data());

	ogls::bindVertexArray(batch->vertexArray);
	ogls::renderDrawIndex(batch->indices.size());
	ogls::bindVertexArray(0);
}

void drawLine(BatchGroup* batch, OglsVec2 pos1, OglsVec2 pos2, OglsVec3 color)
{
	batch->vertices.clear();
	batch->indices.clear();

	batch->vertices.push_back({ pos1, color });
	batch->vertices.push_back({ pos2, color });
	batch->indices = { 0, 1, 2, 3 };

	ogls::bindVertexBufferSubData(batch->vertexBuffer, batch->vertices.size() * sizeof(Vertex), 0, (float*)batch->vertices.data());
	ogls::bindIndexBufferSubData(batch->indexBuffer, batch->indices.size() * sizeof(uint32_t), 0, batch->indices.data());

	ogls::bindVertexArray(batch->vertexArray);
	ogls::renderDrawIndexMode(GL_LINES, batch->indices.size());
	ogls::bindVertexArray(0);
}

void drawTrail(BatchGroup* batch, OglsVec2 pos, OglsVec3 color)
{
	if (batch->vertices.size() >= s_MaxTrailVertices) { batch->vertices.erase(batch->vertices.begin()); } // bruteforce inefficient

	batch->vertices.push_back({pos, color});

	ogls::bindVertexBufferSubData(batch->vertexBuffer, batch->vertices.size() * sizeof(Vertex), 0, (float*)batch->vertices.data());

	ogls::bindVertexArray(batch->vertexArray);
	ogls::renderDrawMode(GL_LINE_STRIP, 0, batch->vertices.size());
	ogls::bindVertexArray(0);
}

float clampAngle(float x)
{
	float angle = std::fmod(x, 2 * PI);
	if (angle < 0) { angle += 2 * PI; }
	return angle;
}


struct Planet
{
	float mass;          // mass of planet
	float distance;      // distance from sun
	float radius;        // radius of planet, (visual only)
	OglsVec2 pos;        // x, y pos of planet
	OglsVec2 vel;        // x, y velocity of planet
	OglsVec3 color;
	bool sun;

	OglsVertexBuffer* vertexBuffer;
	OglsVertexArray* vertexArray;
	BatchGroup trailBatch;
};

void initPlanet(Planet* planet, float mass, float distance, float radius, const OglsVec2& pos, const OglsVec2 vel, const OglsVec3& color, bool sun = false)
{
	*planet = { mass, distance, radius, pos, vel, color, sun };

	ogls::createVertexBuffer(&planet->vertexBuffer, nullptr, sizeof(Vertex) * s_MaxTrailVertices, Ogls_BufferMode_Dynamic);

	std::vector<OglsVertexArrayAttribute> attributePtrs =
	{
		{ 0, 2, sizeof(Vertex), Ogls_DataType_Float, (void*)0 },
		{ 1, 3, sizeof(Vertex), Ogls_DataType_Float, (void*)(2 * sizeof(float)) },
	};

	OglsVertexArrayCreateInfo trailVertexArrayCreatInfo{};
	trailVertexArrayCreatInfo.vertexBuffer = planet->vertexBuffer;
	trailVertexArrayCreatInfo.indexBuffer = nullptr;
	trailVertexArrayCreatInfo.pAttributes = attributePtrs.data();
	trailVertexArrayCreatInfo.attributeCount = attributePtrs.size();

	ogls::createVertexArray(&planet->vertexArray, &trailVertexArrayCreatInfo);

	BatchGroup batchTrail{};
	batchTrail.vertexBuffer = planet->vertexBuffer;
	batchTrail.vertexArray = planet->vertexArray;
	planet->trailBatch = batchTrail;
}

void uninitPlanet(Planet* planet)
{
	ogls::destroyVertexBuffer(planet->vertexBuffer);
	ogls::destroyVertexArray(planet->vertexArray);
}

OglsVec2 getPlanetAttraction(const Planet& p1, const Planet& p2)
{
	float distx = p1.pos.x - p2.pos.x;
	float disty = p1.pos.y - p2.pos.y;

	float distance = std::sqrt(std::pow(distx, 2) + std::pow(disty, 2));
	float force = G_CONSTANT * p1.mass * p2.mass / std::pow(distance, 2);
	float theta = std::atan2(disty, distx);

	float fx = force * std::cos(theta);
	float fy = force * std::sin(theta);

	return { -fx, -fy };
}


int main(int argv, char** argc)
{
	if (!glfwInit())
	{
		printf("failed to initialize glfw\n");
		return -1;
	}

	printf("glfw initialized\n");

	GLFWwindow* window = glfwCreateWindow(1280, 800, "solar system", NULL, NULL);
	if (!window)
	{
		printf("failed to create window!\n");
		glfwTerminate();
		return -1;
	}

	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwMakeContextCurrent(window);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		printf("failed to initialize glad!\n");
		return -1;
	}

	printf("%s\n", "glad initialized\n");

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 330 core");


	// setup opengl buffers
	std::vector<OglsVertexArrayAttribute> attributePtrs =
	{
		{ 0, 2, sizeof(Vertex), Ogls_DataType_Float, (void*)0 },
		{ 1, 3, sizeof(Vertex), Ogls_DataType_Float, (void*)(2 * sizeof(float)) },
	};

	OglsVertexBuffer* vertexBuffer;
	ogls::createVertexBuffer(&vertexBuffer, nullptr, sizeof(Vertex) * s_MaxVertices, Ogls_BufferMode_Dynamic);

	OglsIndexBuffer* indexBuffer;
	ogls::createIndexBuffer(&indexBuffer, nullptr, sizeof(uint32_t) * s_MaxIndices, Ogls_BufferMode_Dynamic);

	OglsVertexArrayCreateInfo vertexArrayCreatInfo{};
	vertexArrayCreatInfo.vertexBuffer = vertexBuffer;
	vertexArrayCreatInfo.indexBuffer = indexBuffer;
	vertexArrayCreatInfo.pAttributes = attributePtrs.data();
	vertexArrayCreatInfo.attributeCount = attributePtrs.size();

	OglsVertexArray* vertexArray;
	ogls::createVertexArray(&vertexArray, &vertexArrayCreatInfo);



	// setup opengl shader
	OglsShaderCreateInfo shaderCreateInfo{};
	shaderCreateInfo.vertexSrc = vertexShaderSource;
	shaderCreateInfo.fragmentSrc = fragmentShaderSource;

	OglsShader* shader;
	ogls::createShaderFromStr(&shader, &shaderCreateInfo);


	// setup batch groups
	BatchGroup batch{};
	batch.vertexBuffer = vertexBuffer;
	batch.indexBuffer = indexBuffer;
	batch.vertexArray = vertexArray;
	

	// solar system code

	// planet initialization
	Planet sun;
	initPlanet(&sun, 1.9891e+30, 0.0f, 35.0f, {0.0f, 0.0f}, {0.0f, 0.0f}, { SUN_COLOR }, true);

	Planet mercury;
	initPlanet(&mercury, 0.330e+24, 0.387 * AU, 4.0f, {0.387f * AU, 0.0f}, {0.0f, 47400.0f}, { MERCURY_COLOR });

	Planet venus;
	initPlanet(&venus, 4.98e+24, 0.72f * AU, 10.0f, {0.72f * AU, 0.0f}, {0.0f, 35000.0f}, { VENUS_COLOR });

	Planet earth;
	initPlanet(&earth, 5.97e+24, AU, 11.0f, {AU, 0.0f}, {0.0f, 29800.0f}, { EARTH_COLOR });

	Planet mars;
	initPlanet(&mars, 0.642e+24, 1.5f * AU, 8.0f, {1.5f * AU, 0.0f}, {0.0f, 24100.0f}, { MARS_COLOR });

	Planet jupiter;
	initPlanet(&jupiter, 1868e+24, 5.2f * AU, 30.0f, {5.2f * AU, 0.0f}, {0.0f, 13100.0f}, { JUPITER_COLOR });

	Planet saturn;
	initPlanet(&saturn, 568e+24, 9.5f * AU, 28.0f, {9.5f * AU, 0.0f}, {0.0f, 9700.0f}, { SATURN_COLOR });

	Planet uranus;
	initPlanet(&uranus, 86.8e+24, 19.0f * AU, 18.0f, {19.0f * AU, 0.0f}, {0.0f, 6800.0f}, { URANUS_COLOR });

	Planet neptune;
	initPlanet(&neptune, 102e+24, 30.0f * AU, 18.0f, {30.0f * AU, 0.0f}, {0.0f, 5400.0f}, { NEPTUNE_COLOR });

	Planet pluto;
	initPlanet(&pluto, 0.0130e+24, 39.0f * AU, 3.0f, {39.0f * AU, 0.0f}, {0.0f, 4700.0f}, { PLUTO_COLOR });

	std::vector<Planet> planets = { sun, mercury, venus, earth, mars, jupiter, saturn, uranus, neptune, pluto };
	std::vector<Planet> planetCopies = planets;


	float camx = 0.0f, camy = 0.0f;
	float scale = 1.0f;
	float timeStep = 86400;
	bool p_open = false, pressOnce = false;
	bool trailPaths = false;
	ImGuiTableFlags flags = ImGuiTableFlags_RowBg;

	bool pause = false;
	std::string pauseName = "pause";

	auto timer = std::chrono::high_resolution_clock::now();
	float oldTime = 0.0f;
	Timer deltaTime;
	deltaTime.start();

	glViewport(0, 0, 1280, 800);

	printf("Press \'c\' to open the settings menu\n");

    while (!glfwWindowShouldClose(window))
    {
		float timeNow = deltaTime.elapsed();
		float dt = timeNow - oldTime;
		oldTime = timeNow;

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();


		// begin render
		glClearColor(COLOR_BG, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		int width, height;
		glfwGetWindowSize(window, &width, &height);

		cameraMovement(window, &camx, &camy, dt);
		cameraScale(window, &scale, dt);

		glm::mat4 proj = glm::ortho(-static_cast<float>(width) * 0.5f * scale, static_cast<float>(width) * 0.5f * scale, -static_cast<float>(height) * 0.5f * scale, static_cast<float>(height) * 0.5f * scale);
		glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(camx, camy, 0.0f));
		glm::mat4 camera = proj * view;

		ogls::bindShader(shader);
		glUniformMatrix4fv(glGetUniformLocation(ogls::getShaderId(shader), "u_Camera"), 1, GL_FALSE, glm::value_ptr(camera));

		// calculate planet positions and forces
		if (!pause)
		{
			for (int i = 0; i < planets.size(); i++)
			{
				auto& planet = planets[i];
				OglsVec2 sumOfForces = {0.0f, 0.0f};

				for (int j = 0; j < planets.size(); j++)
				{
					if (i == j) continue;
					if (planets[j].sun)
					{
						planet.distance = std::sqrt(std::pow(planet.pos.x - planets[j].pos.x, 2) + std::pow(planet.pos.y - planets[j].pos.y, 2));
					}

					OglsVec2 f = getPlanetAttraction(planet, planets[j]);
					sumOfForces.x += f.x;
					sumOfForces.y += f.y;
				}

				planet.vel.x += sumOfForces.x / planet.mass * timeStep;
				planet.vel.y += sumOfForces.y / planet.mass * timeStep;

				planet.pos.x += planet.vel.x * timeStep;
				planet.pos.y += planet.vel.y * timeStep;
			}
		}

		if (trailPaths)
		{
			for (auto& planet : planets)
			{
				drawTrail(&planet.trailBatch, { planet.pos.x * SCREEN_SCALE, planet.pos.y * SCREEN_SCALE }, {TRAIL_LINE_COLOR});
			}
		}
		for (auto& planet : planets)
		{
			drawPoly(&batch, { planet.pos.x * SCREEN_SCALE, planet.pos.y * SCREEN_SCALE }, planet.color, planet.radius, 32);
		}


		// imgui
		int key = glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS;
		if (key == GLFW_PRESS && !pressOnce)
		{
			if (!p_open)
				p_open = true;
			else
				p_open = false;
			
			pressOnce = true;
		}
		else if (key == GLFW_RELEASE)
			pressOnce = false;

		if (p_open)
		{
			ImGui::Begin("Settings", &p_open);
			ImGui::Text("Solar System Simulation in OpenGL and C++");
			ImGui::Text("- Use (wasd) to move the camera around");
			ImGui::Text("- Press (-) and (+) to zoom in and out");
			ImGui::Text("- Hold down shift to increase speed and zoom");
			ImGui::Text("- Note that Planet sizes are not proportional to real life");
			ImGui::NewLine();

			if (ImGui::BeginTable("table2", 4, flags))
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::Text("Planet/Star");
				ImGui::TableNextColumn();
				ImGui::Text("Mass (kg)");
				ImGui::TableNextColumn();
				ImGui::Text("Distance (AU)");
				ImGui::TableNextColumn();
				ImGui::Text("Velovity (m/s)");

				for (int i = 0; i < planets.size(); i++)
				{
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::Text("body %d:", i);
					ImGui::TableNextColumn();
					ImGui::Text("%.3e", planets[i].mass);
					ImGui::TableNextColumn();
					ImGui::Text("%.3e", planets[i].distance);
					ImGui::TableNextColumn();
					ImGui::Text("x:%.2f, y:%.2f", planets[i].vel.x, planets[i].vel.y);
				}
				ImGui::EndTable();
			}

			ImGui::NewLine();
			ImGui::Text("Options");
			ImGui::DragFloat("zoom", &scale, 0.5f, 0.5f);
			ImGui::DragFloat("time step", &timeStep, 10.0f, 60.0f);
			if (ImGui::Checkbox("trail paths", &trailPaths))
			{
				for (auto& planet : planets)
				{
					planet.trailBatch.vertices.clear();
					planet.trailBatch.indices.clear();
				}
			}
			if (ImGui::Button("Clear trail paths"))
			{
				for (auto& planet : planets)
				{
					planet.trailBatch.vertices.clear();
					planet.trailBatch.indices.clear();
				}
			}
			if (ImGui::Button(pauseName.c_str()))
			{
				if (pause)
				{
					pause = false;
					pauseName = "Pause";
				}
				else
				{
					pause = true;
					pauseName = "Play";
				}
			}
			if (ImGui::Button("Restart"))
			{
				planets = planetCopies;
				camx = camy = 0.0f;
				scale = 1.0f;
				timeStep = 86400;
			}

			ImGui::NewLine();
			ImGui::Text("Fun Stuff");
			if(ImGui::Button("Delete the sun"))
			{
				for (int i = 0; i < planets.size(); i++)
				{
					if (planets[i].sun)
					{
						planets.erase(planets.begin() + i);
						goto OUT;
					}

				}
			}
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary | ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay))
				ImGui::SetTooltip("I know gravity takes time to travel through space so\nthe planets should still be orbiting around even after the\nsun is gone (similar to light), but I am too lazy to implement that right now");

			ImGui::SameLine();
			if(ImGui::Button("Delete a random planet"))
			{
				if (planets.empty()) goto OUT;
				if (planets.size() == 1 && planets[0].sun) goto OUT;
				if (planets.size() == 1 ) { planets.erase(planets.begin()); goto OUT; }
				srand(time(0));
				auto index = 1 + rand() % (planets.size() - 1);
				planets.erase(planets.begin() + index);
			}
			OUT:

			if (ImGui::Button("Make the mass of pluto the sun"))
			{
				planets[9].mass = 1.9891e+30;
			}
			if (ImGui::Button("Make all the planets have the mass of the sun"))
			{
				for (auto& planet : planets)
					planet.mass = 1.9891e+30;
			}
			if (ImGui::Button("set all planet velocity to 0"))
			{
				for (auto& planet : planets)
				{
					if (planet.sun) continue;
					planet.vel = { 0.0f, 0.0f };
				}
			}


			ImGui::NewLine();
			ImGui::Text("Time elapsed: %f", std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - timer).count() * 0.001f * 0.001f * 0.001f);

			ImGui::End();
		}

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);
        glfwPollEvents();
    }

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	ogls::destroyShader(shader);
	ogls::destroyVertexArray(vertexArray);
	ogls::destroyIndexBuffer(indexBuffer);
	ogls::destroyVertexBuffer(vertexBuffer);

	for (auto& planet : planets)
	{
		uninitPlanet(&planet);
	}

    glfwTerminate();
    return 0;
}
