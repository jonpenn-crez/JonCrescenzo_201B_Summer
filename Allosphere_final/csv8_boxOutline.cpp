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
  Parameter pointSize{"/pointSize", "", 40.0, 5.0, 600.0};
  Parameter timeStep{"/timeStep", "", 0.02, 0.01, 0.6};
  Parameter dragforce{"/dragforce", "", 0.5, 0.0, 1.0};
  Parameter stiffness{"/stiffness", "", 1.0, 0.1, 5.0};
  Parameter sphereRadius{"/sphereRadius", "", 20.0, 5.0, 40.0};

  Parameter attraction{"/attraction", "", 0.05, 0.0, 0.2};
  Parameter minDistance{"/minDistance", "", 5.0, 0.1, 10.0};
  Parameter repelStrength{"/repelStrength", "", 1.0, 0.0, 20.0};
  Parameter lineWidth{"/lineWidth", "", 4.0, 1.0, 10.0};
  Parameter boxWidth{"/boxWidth", "", 5.0, 0.1, 10.0};
  Parameter boxHeight{"/boxHeight", "", 0.5, 0.1, 3.0};
  Parameter alphaLines{"/Alpha Lines", "", 0.5, 0.1, 1.0};

  Font font;

  Mesh mesh;
  Mesh lineMesh;

  double timer = 0.0;

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
    gui.add(boxWidth);
    gui.add(boxHeight);
    gui.add(alphaLines);

  }

  void onCreate() override {
    font.load("/Users/joncrescenzo/allolib_playground/JonCrescenzo_201B_Summer/Allosphere_final/data/Datatype-VariableFont_wdth,wght.ttf", 200, 1024);
    font.alignCenter();

    loadCSV("/Users/joncrescenzo/allolib_playground/JonCrescenzo_201B_Summer/Allosphere_final/test4.csv");

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
    timer += dt;

    vector<Vec3f> &position = mesh.vertices();

    float floorY = -sphereRadius * 0.5;

    // Different categories repel
    for (int i = 0; i < position.size(); i++) {
      for (int j = i + 1; j < position.size(); j++) {
        if (categories[i] != categories[j]) {
          Vec3f direction = position[i] - position[j];
          float distance = direction.mag();

          if (distance > 0.0001) {
            direction.normalize();

            float separationDistance = 5.0;
            float categoryRepel = 0.5;

            if (distance < separationDistance) {
              Vec3f repelForce = direction * categoryRepel;
              force[i] += repelForce;
              force[j] -= repelForce;
            }
          }
        }
      }
    }

    // Push away from south pole
    for (int i = 0; i < position.size(); i++) {
      if (position[i].y < floorY) {
        float pushStrength = 5.0;
        force[i] += Vec3f(0, pushStrength, 0);
      }
    }

    // Keep all words near sphere surface
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

    // Same categories attract, but repel if too close
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

    // Drag
    for (int i = 0; i < velocity.size(); i++) {
      force[i] += -velocity[i] * dragforce;
    }

    // Integrate
    for (int i = 0; i < velocity.size(); i++) {
      velocity[i] += force[i] / mass[i] * timeStep;
      position[i] += velocity[i] * timeStep;
    }

    // Build connection lines after 12 seconds
    if (timer > 12.0) {
      lineMesh.reset();
      lineMesh.primitive(Mesh::LINES);

      float t = (timer - 12.0) / 3.0;

      if (t < 0) t = 0;
      if (t > 1) t = 1;

      float easeT = t * t * (3 - 2 * t);

      for (int i = 0; i < position.size(); i++) {
        for (int j = i + 1; j < position.size(); j++) {
          if (categories[i] == categories[j]) {
            Vec3f start = position[i];
            Vec3f end = position[j];
            Vec3f animatedEnd = start + (end - start) * easeT;

            Color inlineColor = Color(1, 1, 1, 0.25);

            if (categories[i] == "Intercept") {
              inlineColor = Color(1, 1, 1, alphaLines);
            }

            if (categories[i] == "NYT") {
              inlineColor = Color(1, 1, 1, alphaLines);
            }

            if (categories[i] == "WSJ") {
              inlineColor = Color(1, 1, 1, alphaLines);
            }

            if (categories[i] == "FTTimes") {
              inlineColor = Color(1, 1, 1, alphaLines);
            }

            if (categories[i] == "DropSite") {
              inlineColor = Color(1, 1, 1, alphaLines);
            }

            lineMesh.vertex(start);
            lineMesh.color(inlineColor);

            lineMesh.vertex(animatedEnd);
            lineMesh.color(inlineColor);
          }
        }
      }
    }

    for (auto &f : force) {
      f.set(0);
    }
  }

  void onDraw(Graphics &g) override {
    g.clear(0.3);

    g.blending(true);
    g.blendTrans();
    g.depthTesting(true);

    g.meshColor();
    g.lineWidth(lineWidth);
    g.draw(lineMesh);

    vector<Vec3f> &position = mesh.vertices();

    for (int i = 0; i < textMeshes.size(); i++) {
      float t = (timer - 3.0) / 2.0;

      if (t < 0) t = 0;
      if (t > 1) t = 1;

      float easeB = t * t * (3 - 2 * t);

      Vec3f start(0, 0, 0);
      Vec3f end = position[i];
      Vec3f animatedPos = start + (end - start) * easeB;

      g.pushMatrix();

      g.translate(animatedPos);

      Quatd rotation = Quatd::getBillboardRotation(
          -position[i] / position[i].mag(),
          Vec3d(0, 1, 0));

      g.rotate(rotation);

      // Colored outline box
      if (timer > 2.5) {
        Mesh outline;
        outline.primitive(Mesh::LINE_LOOP);

        float w = boxWidth;;
        float h = boxHeight;

        Color boxColor = Color(1, 1, 1);

        if (categories[i] == "Intercept") {
          boxColor = Color(1, 0, 0);
        }

        if (categories[i] == "FTT") {
          boxColor = Color(1, 0, 1);
        }

        if (categories[i] == "NYT") {
          boxColor = Color(1, 1, 0);
        }

        if (categories[i] == "DropSite") {
          boxColor = Color(1, 0.5, 1);
        }
         if (categories[i] == "WSJ") {
          boxColor = Color(0, 0.5, 1);
        }


        outline.vertex(-w, -h, 0);
        outline.color(boxColor);

        outline.vertex(w, -h, 0);
        outline.color(boxColor);

        outline.vertex(w, h, 0);
        outline.color(boxColor);

        outline.vertex(-w, h, 0);
        outline.color(boxColor);

        g.lineWidth(1.0);
        g.meshColor();
        g.draw(outline);
      }

      g.texture();
      font.tex.bind();

      g.scale(pointSize / 5.0);

      if (timer > 3.0) {
        g.draw(textMeshes[i]);
      }

      font.tex.unbind();

      g.popMatrix();
    }
  }
};

int main() {
  AlloApp app;
  app.start();
}