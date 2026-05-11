#include "al/app/al_App.hpp"
#include "al/app/al_GUIDomain.hpp"
#include "al/math/al_Random.hpp"
#include "al/graphics/al_Font.hpp"

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
  Parameter pointSize{"/pointSize", "", 10.0, 1.0, 12.0};     // size of points
  Parameter timeStep{"/timeStep", "", 0.02, 0.01, 0.6};      // simulation speed
  Parameter dragforce{"/dragforce", "", 0.9, 0.0, 1.0};    // damping
  Parameter stiffness{"/stiffness", "", 1.0, 0.1, 5.0};      // spring strength
  Parameter sphereRadius{"/sphereRadius", "", 10.0, 1.0, 20.0}; // desired radius

  // ShaderProgram pointShader;

//declare font
  Font font;

  // Simulation data
  Mesh mesh;                 // holds positions (vertices)
  vector<Vec3f> velocity;   // velocity of each point
  vector<Vec3f> force;      // accumulated forces on each point
  vector<float> mass;       // mass of each point

  vector<string> words = {
      "ONE", "TWO", "THREE", "FOUR", "FIVE",
      "SIX", "SEVEN", "EIGHT", "NINE", "TEN", "ONE", "TWO", "THREE", "FOUR", "FIVE",
      "SIX", "SEVEN", "EIGHT", "NINE", "TEN", "ONE", "TWO", "THREE", "FOUR", "FIVE",
      "SIX", "SEVEN", "EIGHT", "NINE", "TEN"
  };

  //meshes, one per text string 
  vector<Mesh> textMeshes;


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

    //load the font 
    font.load("data/VeraMono.ttf", 200, 1024);

    mesh.primitive(Mesh::POINTS); 

    //number of particles = number of words
    int numberPoints = words.size();
    
    for (int i = 0; i < numberPoints; i++){
      //random point in space, vector array, position on the sphere
      Vec3f sphere = randomVec3f(1.0);
      sphere.normalize(sphereRadius);

      //add points to mesh

    // random color
    // mesh.color(HSV(rnd::uniform(), 1.0, 1.0));

    
    //store the position in mesh
    mesh.vertex(sphere);
    
    
    //why do you want me to get ride the velocity? 
    // because we are going to update the position of the points with a sin wave, so we don't need the velocity to move the points around

    // initializing pyshics
    velocity.push_back(randomVec3f(0.1));
    force.push_back(Vec3f(0, 0, 0));
    mass.push_back(1.0);

  //create a mesh for the words
  Mesh textMesh;
  //need to c_str() to convert from string to char array
  font.write(textMesh, words[i].c_str(), 0.2f);

  //store it 
  textMeshes.push_back(textMesh);

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
        float stretch = distance - sphereRadius;
        Vec3f springforce = -stiffness * stretch * direction;
        force[i] += springforce;
  
        // //expand the sphere with time
        // position[i] += direction * sin(dt);

      }
  }


    //drag forces -- push the velocity back to zero
     for (int i =0; i < velocity.size(); i++) {
      force[i] += -velocity[i] * dragforce;  
      // position[i] += velocity[i] * timeStep;    
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

    // g.shader(pointShader);
    // g.shader().uniform("pointSize", pointSize / 100);

    g.blending(true);
    g.blendTrans();
    g.depthTesting(true);

    //bind the textture (needed for text rendering)
    g.texture();
    font.tex.bind();

    vector<Vec3f> &position = mesh.vertices();

    //loop through each particle
    for (int i = 0; i < textMeshes.size(); i++){

      g.pushMatrix();

       // Move to particle position
      g.translate(position[i]);

      // Scale text size
      g.scale(pointSize / 5.0);

      // Draw the text mesh at that position
      g.draw(textMeshes[i]);

      g.popMatrix();

    }

    font.tex.unbind();
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