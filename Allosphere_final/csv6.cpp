#include "al/app/al_App.hpp"
#include "al/app/al_GUIDomain.hpp"
#include "al/graphics/al_Font.hpp"
#include "al/math/al_Random.hpp"

#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

using namespace al;
using namespace std;

Vec3f randomVec3f(float scale) {
  return Vec3f(rnd::uniformS(), rnd::uniformS(), rnd::uniformS()) * scale;
}

struct AlloApp : App {
  Parameter pointSize{"/pointSize", "", 8.0, 1.0, 30.0};
  Parameter timeStep{"/timeStep", "", 0.02, 0.01, 0.6};
  Parameter dragforce{"/dragforce", "", 0.5, 0.0, 1.0};
  Parameter stiffness{"/stiffness", "", 1.0, 0.1, 5.0};
  Parameter sphereRadius{"/sphereRadius", "", 10.0, 1.0, 20.0};

  Parameter attraction{"/attraction", "", 0.05, 0.0, 0.2};
  Parameter minDistance{"/minDistance", "", 2.0, 0.1, 10.0};
  Parameter repelStrength{"/repelStrength", "", 0.1, 0.0, 1.0};
  Parameter lineWidth{"/lineWidth", "", 2.0, 1.0, 10.0};

  Font font;

  Mesh mesh;
  Mesh lineMesh;

  vector<Vec3f> velocity;
  vector<Vec3f> force;
  vector<float> mass;

  vector<string> words;
  vector<string> categories;
  vector<Mesh> textMeshes;

  void loadCSV(string filename) {
    ifstream file(filename);

    if (!file.is_open()) {
      cout << "Could not open CSV file: " << filename << endl;
      return;
    }

    string line;

    while (getline(file, line)) {
      stringstream ss(line);

      string title;
      string category;

      getline(ss, title, ',');
      getline(ss, category, ',');

      if (title.size() > 0) {
        words.push_back(title);
        categories.push_back(category);
      }
    }

    file.close();
    cout << "Loaded rows: " << words.size() << endl;
  }

  void onInit() override {
    auto GUIdomain = GUIDomain::enableGUI(defaultWindowDomain());
    auto &gui = GUIdomain->newGUI();

    gui.add(pointSize);
    gui.add(timeStep);
    gui.add(dragforce);
    gui.add(stiffness);
    gui.add(sphereRadius);
    gui.add(attraction);
    gui.add(minDistance);
    gui.add(repelStrength);
    gui.add(lineWidth);
  }

  void onCreate() override {
    font.load("data/Datatype-VariableFont_wdth,wght.ttf", 200, 1024);
    font.alignCenter();

    loadCSV("/Users/joncrescenzo/allolib_playground/JonCrescenzo_201B_Summer/Allosphere_final/test3.csv");

    if (words.size() == 0) {
      words.push_back("NO CSV");
      categories.push_back("none");
    }

    mesh.primitive(Mesh::POINTS);
    lineMesh.primitive(Mesh::LINES);

    int numberPoints = words.size();

    for (int i = 0; i < numberPoints; i++) {
      Vec3f sphere = randomVec3f(1.0);
      sphere.normalize(sphereRadius);

      mesh.vertex(sphere);

      velocity.push_back(randomVec3f(0.1));
      force.push_back(Vec3f(0, 0, 0));
      mass.push_back(1.0);

      Mesh textMesh;
      font.write(textMesh, words[i].c_str(), 0.05f);
      textMeshes.push_back(textMesh);
    }

    nav().pos(0, 0, 40);
  }

  void onAnimate(double dt) override {
    vector<Vec3f> &position = mesh.vertices();

    // Keep all words near the sphere surface
    for (int i = 0; i < position.size(); i++) {
      Vec3f direction = position[i];
      float distance = direction.mag();

      if (distance > 0.0001) {
        direction.normalize();

        float stretch = distance - sphereRadius;
        Vec3f springforce = -stiffness * stretch * direction;

        force[i] += springforce;
      }
    }

    // Matching categories attract, but repel if too close
    
    for (int i = 0; i < position.size(); i++) {
      for (int j = i + 1; j < position.size(); j++) {
        if (categories[i] == categories[j]) {
        Vec3f direction = position[j] - position[i];
          float distance = direction.mag();

          if (distance > 0.0001) {
            direction.normalize();

            Vec3f pairForce;

            if (distance < minDistance) {
              pairForce = -direction * repelStrength;
            } else {
              pairForce = direction * attraction;
            }

            force[i] += pairForce;
            force[j] -= pairForce;
          }
        }
      }
    }

    // Drag slows motion
    for (int i = 0; i < velocity.size(); i++) {
      force[i] += -velocity[i] * dragforce;
    }

    // Update velocity and position
    for (int i = 0; i < velocity.size(); i++) {
      velocity[i] += force[i] / mass[i] * timeStep;
      position[i] += velocity[i] * timeStep;
    }

    // Build line mesh ONCE per frame
    lineMesh.reset();
    lineMesh.primitive(Mesh::LINES);

    for (int i = 0; i < position.size(); i++) {
      for (int j = i + 1; j < position.size(); j++) {
        if (categories[i] == categories[j]) {
          
          //color lines based on category
          Color inlineColor = Color(1, 1, 1);
          
          if(categories[i] == "Intercept"){
              inlineColor = Color(1, 0, 0);
            }

            if (categories[i] == "The NY Times"){
              inlineColor = Color(0, 1, 0);
            }

            if (categories[i] == "WSJ"){
              inlineColor = Color(0, 1, 1);
            }

            if (categories[i] == "FTTimes"){
              inlineColor = Color (0.25, 1, 0.5);
            }

          lineMesh.vertex(position[i]);
          lineMesh.color(inlineColor);

          lineMesh.vertex(position[j]);
          lineMesh.color(inlineColor);
        }
      }
    }

    // Clear forces
    for (auto &f : force) {
      f.set(0);
    }
  }

  void onDraw(Graphics &g) override {
    g.clear(0.3);

    g.blending(true);
    g.blendTrans();
    g.depthTesting(true);

    // Draw lines first
    // g.texture(false);
    g.meshColor();
    g.lineWidth(lineWidth);
    g.draw(lineMesh);

    // Draw text second
    g.texture();
    font.tex.bind();

    vector<Vec3f> &position = mesh.vertices();

    for (int i = 0; i < textMeshes.size(); i++) {
      g.pushMatrix();

      g.translate(position[i]);

      Quatd rotation = Quatd::getBillboardRotation(
          -position[i] / position[i].mag(),
          Vec3d(0, 1, 0));
      g.rotate(rotation);

      g.scale(pointSize / 5.0);
      g.draw(textMeshes[i]);

      g.popMatrix();
    }

    font.tex.unbind();
  }
};

int main() {
  AlloApp app;
  app.start();
}