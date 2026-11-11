#define IMGUI_IMPL_OPENGL_ES2

#include <SDL2/SDL.h>
#include <GLES2/gl2.h>

#include "Imgui/imgui.cpp"
#include "Imgui/imgui_draw.cpp"
#include "Imgui/imgui_tables.cpp"
#include "Imgui/imgui_widgets.cpp"
#include "Imgui/imgui_impl_sdl2.cpp"
#include "Imgui/imgui_impl_opengl3.cpp"

// ---- Motor (GSCore) ----
#include "../GSCore/engine.c"
#include "../GSCore/log.c"
#include "../GSCore/objects.c"
#include "../GSCore/gstime.c"
#include "../GSCore/input.c"
#include "../GSCore/collision.c"
#include "../GSCore/render.c"

static Engine engine;   // el motor 

int main(int, char**) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);

    SDL_Window* w = SDL_CreateWindow("ImGui", 0, 0, 0, 0,
        SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_GLContext gl = SDL_GL_CreateContext(w);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;      // activa el docking
    ImGui::GetStyle().FontScaleMain = 2.5f;

    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForOpenGL(w, gl);
    ImGui_ImplOpenGL3_Init("#version 100");

    engine_init(&engine);
    object_create(&engine, "Jugador", OBJ_TYPE_SPRITE, 120, 80, 1);
    object_create(&engine, "Punto", OBJ_TYPE_EMPTY, 40, 40, 0);

    bool run = true;
    int contador = 0;
    static int sel = 1;   // id del objeto 

    while (run) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL2_ProcessEvent(&e);
            if (e.type == SDL_QUIT) run = false;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // el area donde se acoplan las ventanas
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        // ---- Motor ----
        ImGui::Begin("engine motor");
        ImGui::Text("GSEngine");
        if (ImGui::Button("Pulsame")) contador++;
        ImGui::Text("Contador: %d", contador);
        ImGui::Text("FPS: %.1f", io.Framerate);
        ImGui::End();

        // ---- objetos/object----
        ImGui::Begin("Jerarquia");
        for (int i = 0; i < MAX_OBJECTS; i++) {
            Object* o = &engine.objects[i];
            if (!o->active) continue;
            ImGui::PushID(o->id);
            if (ImGui::Selectable(o->name, sel == o->id)) sel = o->id;
            ImGui::PopID();
        }
        ImGui::End();

        // ---- Propiedades/propietie ----
        ImGui::Begin("Propiedades");
        Object* obj = object_find(&engine, sel);
        if (obj) {
            ImGui::Text("%s  (id %d)", obj->name, obj->id);
            if (ImGui::CollapsingHeader("Transformacion", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::DragInt("x", &obj->x);
                ImGui::DragInt("y", &obj->y);
            }
        } else {
            ImGui::TextDisabled("Elige un objeto en la Jerarquia");
        }
        ImGui::End();

        // ---- Consola/console ----
        ImGui::Begin("console");
        ImGui::Text("console log");
        ImGui::Text("Objetos: %d", (int)objects_count(&engine));
        for (int i = 0; i < log_count(&engine.log); i++)
            ImGui::TextUnformatted(log_get(&engine.log, i));
        ImGui::End();

        ImGui::Render();
        int dw, dh;
        SDL_GL_GetDrawableSize(w, &dw, &dh);
        glViewport(0, 0, dw, dh);
        glClearColor(0.1f, 0.1f, 0.15f, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(w);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl);
    SDL_DestroyWindow(w);
    SDL_Quit();
    return 0;
}
