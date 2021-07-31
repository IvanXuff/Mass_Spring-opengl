#include<GL/glew.h>

#define GLEW_STATIC
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "shader.h"

#include <iostream>
#include <vector>
#include <math.h>
 
#include"imgui.h"
#include"imgui_impl_glfw.h"
#include"imgui_impl_opengl3.h"
#include <stdio.h>


float spring_Y = 1000;
bool paused = 0;
int drag_damping = 1;
int dashpot_damping = 100;
float gravity = -9.8;

const int max_num_particles = 1024;
float particle_mass = 1.0;
float dt = 0.001;
int substeps = 10;

int num_particles = 0;
glm::vec3 x[max_num_particles];
glm::vec3 v[max_num_particles];
glm::vec3 f[max_num_particles];

std::vector<bool> fixed(max_num_particles, 0);
std::vector<std::vector<float>> rest_length(max_num_particles, std::vector<float>(max_num_particles, 0.0));


float moux;
float mouy;

float vertices[] = {
    0.0f,0.0f,0.0f,   0.0f,0.0f,0.0f };
unsigned int VBO, VAO, EBO;
//着色器设置

void substep();
void new_particle(float _x, float _y, bool _fixed);
void attract(float x, float y);
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);



const unsigned int SCR_WIDTH = 1200;
const unsigned int SCR_HEIGHT = 900;

int main()
{
    //初始化glfw
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif


    //创建窗口
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    
    //初始化glew
    GLenum err = glewInit();
    if (GLEW_OK != err)
    {
        std::cout << "Failed to initialize GLEW" << std::endl;
        return -1;
    }

    //配置imgui/init imgui
    const char* glsl_version = "#version 130";

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);


    //载入着色器
    Shader ourShader("shader.vs", "shader.fs");

    //绑定VAO,VBO
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);


    ourShader.use();

    new_particle(300, 400, 0);
    new_particle(350, 450, 0);
    new_particle(350, 400, 0);

    glPointSize(10.0f);

    //从标准视图转为符合自己设计的坐标视图
    glm::mat4 projection = glm::mat4(1.0f);
    projection = glm::ortho(0.0f, (float)SCR_WIDTH, 0.0f, (float)SCR_HEIGHT, -1.0f, 1.0f);
    ourShader.setMat4("projection", projection);


    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);



    while (!glfwWindowShouldClose(window))
    {
        //检测键盘输入
        processInput(window);

        //创建imgui窗口
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();


        //配置imgui窗口
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Mass_Spring system");                          // Create a window called "Hello, world!" and append into it.

            ImGui::Text("Use Key:W,S to change spring_Y");
            ImGui::Text("spring_Y : %f",spring_Y);
            ImGui::Text("Use Key:A,D to change damping");
            ImGui::Text("damping : %i", dashpot_damping);
            ImGui::Text("Use Key:Z,X to gravity");
            ImGui::Text("gravity : %f", gravity);

            /*ImGui::SliderFloat("float", &f, 0.0f, 1.0f);   */         // Edit 1 float using a slider from 0.0f to 1.0f
            /*ImGui::ColorEdit3("clear color", (float*)&clear_color);*/ // Edit 3 floats representing a color

            if (ImGui::Button("Restart"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
            {
                num_particles = 0;
                new_particle(100, 200, 0);
            }
            ImGui::SameLine();
            ImGui::Text("OR press R key to restart");

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::End();
        }





        //窗口清理
        glClearColor(0.8f, 0.8f, 0.8f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // 启用着色器
        ourShader.use();

        //开始渲染
        glBindVertexArray(VAO);
        for (int i = 0; i < num_particles; ++i)
        {
            glm::mat4 model(1.0f);
            model = glm::translate(model, x[i]);
            ourShader.setMat4("model", model);

            glDrawArrays(GL_POINTS, 0, 1);
        }

        glm::mat4 model(1.0f);
        ourShader.setMat4("model", model);
        for (int i = 0; i < num_particles; ++i)
        {
            for (int j = i + 1; j < num_particles; ++j)
            {
                if (rest_length[i][j])
                {
                    vertices[0] = x[i].x;
                    vertices[1] = x[i].y;
                    vertices[3] = x[j].x;
                    vertices[4] = x[j].y;
                    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
                    glDrawArrays(GL_LINES, 0, 2);

                }
            }
        }

        for (int i = 0; i < 6; i++)
        {
            vertices[i] = 0;
        }

        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

        for (int i = 0; i < substeps; ++i)
        {
            substep();
        }
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());


        //切换缓冲
        glfwSwapBuffers(window);
        glfwPollEvents();
    }


    //清理内存
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}


void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    {
        spring_Y *= 1.01;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    {
        if (spring_Y > 10)
        spring_Y /= 1.01;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    {
        dashpot_damping += 1;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    {
        if(dashpot_damping>1)
            dashpot_damping -= 1;
    }
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS)
    {
        num_particles = 0;
        new_particle(300, 400, 0);
        new_particle(350, 450, 0);
        new_particle(350, 400, 0);
    }
    if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS)
    {
        gravity *= 1.001;
    }
    if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS)
    {
        gravity /= 1.001;
    }
}


void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void substep()
{
    int n = num_particles;


    for (int i = 0; i < n; ++i)
    {
        //计算重力
        f[i] = glm::vec3(0.0 * particle_mass, gravity * particle_mass, 0.0f);
        for (int j = 0; j < n; ++j)
        {
            if (rest_length[i][j])//如果相连
            {
                glm::vec3 x_ij = x[i] - x[j];
                glm::vec3 d = glm::normalize(x_ij);

                //弹簧力
                f[i] += -spring_Y * (glm::length(x_ij) / rest_length[i][j] - 1.0f) * d;

                float v_rel = glm::dot(v[i] - v[j], d);
                f[i] += -dashpot_damping * v_rel * d;
            }
        }
    }


    for (int i = 0; i < n; ++i)
    {
        //是否固定
        if (!fixed[i])
        {
            v[i] += dt * f[i] / particle_mass;
            v[i] *= std::exp(-dt * drag_damping);

            //移动
            x[i] += v[i] * dt;
        }
        else
        {
            v[i] = glm::vec3(0, 0, 0);
        }

        //边界检测
        if (x[i].y < 0)
        {
            x[i].y = 0;
            v[i].y = 0;
        }
    }

    return;

}

void new_particle(float _x, float _y, bool _fixed)
{
    int new_particle_id = num_particles;
    x[new_particle_id] = glm::vec3(_x, _y, 0);
    v[new_particle_id] = glm::vec3(0, 0, 0);
    fixed[new_particle_id] = _fixed;
    num_particles++;

    for (int i = 0; i < new_particle_id; ++i)
    {
        float dist = glm::length(x[new_particle_id] - x[i]);
        float connection_radius = 100;

        if (dist < connection_radius)
        {
            rest_length[i][new_particle_id] = 70;
            rest_length[new_particle_id][i] = 70;
        }
        else
        {
            rest_length[i][new_particle_id] = 0;
            rest_length[new_particle_id][i] = 0;
        }
    }
}

void attract(float _x, float _y)
{
    for (int i = 0; i < num_particles; ++i)
    {
        glm::vec3 p = { _x,_y,0 };
        v[i] += -dt * substeps * (x[i] - p) * 100.0f;
    }
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    moux = xpos;
    mouy = (float)SCR_HEIGHT - ypos;
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        new_particle(moux, mouy, 0);
    }
}