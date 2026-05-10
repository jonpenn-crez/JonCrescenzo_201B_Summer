#include "al/app/al_App.hpp"
#include "al/app/al_GUIDomain.hpp"
#include "al/math/al_Random.hpp"
#include <vector>

using namespace al;
using namespace std;

// Helper: returns a random 3D vector scaled by 'scale'
Vec3f randomVec3f(float scale) {
  return Vec3f(rnd::uniformS(), rnd::uniformS(), rnd::uniformS()) * scale;
}

// Reads a file into a string (used for shaders)
string slurp(string fileName);

struct AlloApp : App {

  // GUI parameters (sliders)
  Parameter pointSize{"/pointSize", "", 5.0, 1.0, 10.0};     // size of points
  Parameter timeStep{"/timeStep", "", 0.02, 0.01, 0.6};      // simulation speed
  Parameter dragforce{"/dragforce", "", 0.5, 0.0, 1.0};    // damping
  Parameter stiffness{"/stiffness", "", 1.0, 0.1, 5.0};      // spring strength
  Parameter sphereRadius{"/sphereRadius", "", 10.0, 1.0, 20.0}; // desired radius

  ShaderProgram pointShader;

  // Simulation data
  Mesh mesh;                 // holds positions (vertices)
  vector<Vec3f> velocity;   // velocity of each point
  vector<Vec3f> force;      // accumulated forces on each point
  vector<float> mass;       // mass of each point

  bool freeze = false;      // pause simulation

  // Called once at startup (good for GUI setup)
  void onInit() override {
    auto GUIdomain = GUIDomain::enableGUI(defaultWindowDomain());
    auto &gui = GUIdomain->newGUI();

    // Sliders for GUI
    gui.add(pointSize);
    gui.add(timeStep);
    gui.add(dragforce);
    gui.add(stiffness);
    gui.add(sphereRadius);
  }


  void onCreate() override {

    //Karl
    // Compile point shader (for rendering)
    pointShader.compile(slurp("../point-vertex.glsl"),
                        slurp("../point-fragment.glsl"),
                        slurp("../point-geometry.glsl"));

    mesh.primitive(Mesh::POINTS); // we are drawing points

   
    //add points to the mesh, for loop
    int numberPoints = 500;
    
    for (int i = 0; i < numberPoints; i++)
{
      //random point in space, vector array
      Vec3f sphere = randomVec3f(1.0);

      // normalize the point for the sphere radius
      sphere.normalize(sphereRadius);

      //add points to mesh
      mesh.vertex(sphere);

    // random color
    mesh.color(HSV(rnd::uniform(), 1.0, 1.0));

    //texture coordinates 
    mesh.texCoord(1.0, 0.0);

//push back adds a new element to the end of the vector

    //give it an initial velocity
    velocity.push_back(randomVec3f(0.1));

    // give it an initial force 
    force.push_back(Vec3f(0, 0, 0));

    // give it a mass, default 1
    mass.push_back(1.0);

}

    // Move camera back so we can see the sphere
    nav().pos(0, 0, 40);
     
}


   void onAnimate(double dt) override {

      //reference to the positions inside the mesh
      vector<Vec3f> &position = mesh.vertices();

      //loop over their positions
      for (int i = 0; i < position.size(); i++){

        //direction and distance to the center

        Vec3f direction = position[i] ;
        float distance  = direction.mag();

        //avoid divide by zero
        if (distance > 0.0001) {
        //convert to unit direction vector
        direction.normalize();
     

        //distance from desired radius
        // + = too far out, - = too far in
        float stretch = distance - sphereRadius;

        //springforce toward the center
        Vec3f springforce = -stiffness * stretch * direction;

        //add this to the force of the particles
        force[i] += springforce;
  
        //expand the sphere with time
        position[i] += direction * sin(dt);

      }
  }


    //drag forces -- push the velocity back to zero
     for (int i =0; i < velocity.size(); i++) {
      force[i] += -velocity[i] * dragforce;      
    }

    

// update the motion 
     for (int i= 0; i < velocity.size(); i++) {
      //F = ma
      velocity[i] += force[i] / mass[i] * timeStep;

      //update position from the velocities
      position[i] += velocity[i] * timeStep;

     }

    // --- CLEAR FORCES ---
    for (auto &f : force) {
      f.set(0); // reset for next frame
    }
  }
  

  // Draw everything
  void onDraw(Graphics &g) override {
    g.clear(0.3);

    g.shader(pointShader);
    g.shader().uniform("pointSize", pointSize / 100);

    g.blending(true);
    g.blendTrans();
    g.depthTesting(true);

    g.draw(mesh);
  }
};

// Main entry point
int main() {
  AlloApp app;
  app.start();
}

// Read file into string
string slurp(string fileName) {
  fstream file(fileName);
  string out = "";

  while (file.good()) {
    string line;
    getline(file, line);
    out += line + "\n";
  }

  return out;
}