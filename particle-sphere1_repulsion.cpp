// Karl Yerkes
// 2022-01-20

#include "al/app/al_App.hpp"
#include "al/app/al_GUIDomain.hpp"
#include "al/math/al_Random.hpp"
#include <cstdio>
#include <type_traits>

using namespace al;

#include <fstream>
#include <vector>
using namespace std;

Vec3f randomVec3f(float scale) {
  return Vec3f(rnd::uniformS(), rnd::uniformS(), rnd::uniformS()) * scale;
}
string slurp(string fileName);  // forward declaration


struct AlloApp : App {
  Parameter pointSize{"/pointSize", "", 5.0, 1.0, 10.0};
  Parameter timeStep{"/timeStep", "", 0.02, 0.01, 0.6};
  Parameter dragFactor{"/dragFactor", "", 0.1, 0.0, 0.9};
  Parameter repulsivity{"/repulsivity", "", 1., 0.1, 5};
  Parameter stiffness{"/stiffness", "", 1., 0.1, 5};
  Parameter restingLength{"/restingLength", "", 1., 0.1, 5};  
  
  //

  ShaderProgram pointShader;

  //  simulation state
  Mesh mesh;  // position *is inside the mesh* mesh.vertices() are the positions
  vector<Vec3f> velocity;
  vector<Vec3f> force;
  vector<float> mass;
  

  void onInit() override {
    // set up GUI
    auto GUIdomain = GUIDomain::enableGUI(defaultWindowDomain());
    auto &gui = GUIdomain->newGUI();
    gui.add(pointSize);  // add parameter to GUI
    gui.add(timeStep);   // add parameter to GUI
    gui.add(dragFactor);   // add parameter to GUI
    gui.add(repulsivity);   // add parameter to GUI
    gui.add(stiffness);   // add parameter to GUI
    gui.add(restingLength);   // add parameter to GUI
  }

  void onCreate() override {
    // compile shaders
    pointShader.compile(slurp("../point-vertex.glsl"),
                        slurp("../point-fragment.glsl"),
                        slurp("../point-geometry.glsl"));

    // set initial conditions of the simulation
    //

    // c++11 "lambda" function
    auto randomColor = []() { return HSV(rnd::uniform(), 1.0f, 1.0f); };

    mesh.primitive(Mesh::POINTS);
    // does 1000 work on your system? how many can you make before you get a low
    // frame rate? do you need to use <1000?
    for (int _ = 0; _ < 500; _++) {
      mesh.vertex(randomVec3f(5));
      mesh.color(randomColor());

      // float m = rnd::uniform(3.0, 0.5);
      float m = 3 + rnd::normal() / 2;
      if (m < 0.5) m = 0.5;
      mass.push_back(m);

      // using a simplified volume/size relationship
      mesh.texCoord(pow(m, 1.0f / 3), 0);  // s, t

      // separate state arrays
      velocity.push_back(randomVec3f(0.1));
      force.push_back(randomVec3f(1));
    }

    // for (int i = 0; i < mesh.vertices().size(); i++) {
    //   mesh.vertices()[i].normalize();
    // }

    nav().pos(0, 0, 40);
  }

  bool freeze = false;
  void onAnimate(double dt) override {
    if (freeze) return;

    // 
    //
    // Vec3f has lots of operations you might use...
    // • +=
    // • -=
    // • +
    // • -
    // • .normalize() ~ Vec3f points in the direction as it did, but has length 1
    // • .normalize(float scale) ~ same but length `scale`
    // • .mag() ~ length of the Vec3f
    // • .magSqr() ~ squared length of the Vec3f
    // • .dot(Vec3f f) 
    // • .cross(Vec3f f)
    for (int i = 0; i < velocity.size(); i++) {
      auto& me = mesh.vertices()[i];
      // attach this particle to the origin with a sping of some stiffness (k) with a resting length of 1
      // calculate spring force between this particle and the origin
      // make stiffness a parameter
      // force_amount == -(resting_length - actual_length) * stiffness
      // 
      // what is the unit vector that points in the direction of force?
      
      //the unit vector is the spring force
      Vec3f force_direction = me;
      force_direction.normalize();
      // force_direction *= -1; // point towards the origin
      float distance = me.mag();
      // printf("distance from origin: %f\n", distance);
      float stiffness = 1;
      float resting_length =1;
      float springForce = - (resting_length - distance) * stiffness; 
      
      printf("spring force: %f\n", springForce);
      
      // point the spring towards the origin
      springForce *= -1;
      // printf("spring force: %f\n", springForce);
     
      //force direction is added to the spring forces 
      force[i] += force_direction * springForce; // add the spring force to the total force on this particle
      
       // point towards the origin
      // force_direction pooints in the direction of spring force
    
      // ????

      // force[i] += 

    }

    // Calculate repulsive forces....
    //
    for (int i = 0; i < mesh.vertices().size(); ++i) {
      for (int j = i + 1; j < mesh.vertices().size(); ++j) {
        // what is the *direction* of the force? (with magnitude of 1)

        Vec3f force_direction = (mesh.vertices()[i] - mesh.vertices()[j]);
        float distance = force_direction.mag(); // aka "r"
        force_direction /= distance; // the same as .normalize() in this case

        float force_amount = repulsivity * mass[i] * mass[j] / (distance * distance);

        // i and j are a pair
        // limit large forces... if the force is too large, ignore it

        if (force_amount > 10) {
          force_amount = 10;
        }

        force[i] += force_direction * force_amount;
        force[j] -= force_direction * force_amount;

          // printf("force between %d and %d: force_amount: %f\n", i, j, force_amount);


      }
    }

///////// you probably do not need to edit the rest of this method...

    // viscous drag
    //
    for (int i = 0; i < velocity.size(); i++) {
      force[i] += - velocity[i] * dragFactor; // F = -bv
    }

    

    // Numerical Integration
    //
    vector<Vec3f> &position(mesh.vertices());
    for (int i = 0; i < velocity.size(); i++) {
      // "semi-implicit" Euler integration
      velocity[i] += force[i] / mass[i] * timeStep;
      position[i] += velocity[i] * timeStep;
    }

    // clear all accelerations (IMPORTANT!!)
    //
    for (auto &a : force) a.set(0);
  }
  

  bool onKeyDown(const Keyboard &k) override {
    if (k.key() == ' ') {
      freeze = !freeze;
    }

    if (k.key() == '1') {
      // introduce some "random" forces
      for (int i = 0; i < velocity.size(); i++) {
        // F = ma
        force[i] += randomVec3f(1);
      }
    }

    return true;
  }

    // bool onMouseDown(const Mouse &m) override {
    // // add a new particle where the mouse is
    // Vec3f newPos = nav().worldToLocal(m.x(), m.y());
    // mesh.vertex(newPos);
    // mesh.color(HSV(rnd::uniform(), 1.0f, 1.0f));
    // mass.push_back(10);
    // velocity.push_back(randomVec3f(0.1));
    // force.push_back(randomVec3f(1));
    // return true;
    // }

    bool mouseDown = false;
    bool onMouseDown(const Mouse &m) override {
      mouseDown = true;
      return true;
      printf("mouse down\n");
    }

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

int main() {
  AlloApp app;
  app.configureAudio(48000, 512, 2, 0);
  app.start();
}

string slurp(string fileName) {
  fstream file(fileName);
  string returnValue = "";
  while (file.good()) {
    string line;
    getline(file, line);
    returnValue += line + "\n";
  }
  return returnValue;
}
