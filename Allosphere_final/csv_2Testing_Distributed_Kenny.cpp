#include "al/app/al_App.hpp"
#include "al/app/al_GUIDomain.hpp"
#include "al/app/al_DistributedApp.hpp"
#include "al/graphics/al_Font.hpp"
#include "al/math/al_Random.hpp"
#include "al/io/al_File.hpp"
#include "al_ext/statedistribution/al_CuttleboneStateSimulationDomain.hpp"

#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

using namespace al;
using namespace std;

const int MAX_HEADLINES = 500;

Vec3f randomVec3f(float scale) {
  return Vec3f(rnd::uniformS(), rnd::uniformS(), rnd::uniformS()) * scale;
}

struct WorldState {
  double timer;
  int count;
  Vec3f positions[MAX_HEADLINES];
};

struct AlloApp : DistributedAppWithState<WorldState> {
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
  Parameter alphaLines{"/alphaLines", "", 0.5, 0.1, 1.0};

  Font font;
  Mesh mesh;
  Mesh lineMesh;
  SearchPaths searchPaths;

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

      if (title.size() > 0 && words.size() < MAX_HEADLINES) {
        words.push_back(title);
        categories.push_back(category);
      }
    }

    file.close();
    cout << "Loaded rows: " << words.size() << endl;
  }

  void onInit() override {
    auto cuttleboneDomain = CuttleboneStateSimulationDomain<WorldState>::enableCuttlebone(this);
    if (!cuttleboneDomain) {
      std::cerr << "WARNING: cuttlebone failed to start" << std::endl;
    }

    if (isPrimary()) {
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
  }

  void onCreate() override {
    searchPaths.addAppPaths();
    searchPaths.addRelativePath("../data");

    font.load(searchPaths.find("Datatype-VariableFont_wdth,wght.ttf").filepath().c_str(), 200, 1024);
    font.alignCenter();

    // In distributed mode, every renderer should load the same CSV/font locally.
    loadCSV(searchPaths.find("test5.csv").filepath());

    if (words.size() == 0) {
      words.push_back("NO CSV");
      categories.push_back("none");
    }

    mesh.primitive(Mesh::POINTS);
    lineMesh.primitive(Mesh::LINES);

    for (int i = 0; i < words.size(); i++) {
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

    state().timer = 0.0;
    state().count = words.size();

    for (int i = 0; i < words.size(); i++) {
      state().positions[i] = mesh.vertices()[i];
    }

    nav().pos(0, 0, 40);

    parameterServer() << pointSize << boxWidth << boxHeight << lineWidth << alphaLines;
  }

  Color categoryColor(string c) {
    if (c == "Intercept") return Color(1, 1, 1);
    if (c == "FTT") return Color(1, 1, 1);
    if (c == "NYT") return Color(1, 1, 0);
    if (c == "DropSite") return Color(1, 0.5, 1);
    if (c == "WSJ") return Color(0, 0.5, 1);

    return Color(1, 1, 1);
  }

  void onAnimate(double dt) override {
    vector<Vec3f> &position = mesh.vertices();

    if (isPrimary()) {
      timer += dt;

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
          force[i] += Vec3f(0, 5.0, 0);
        }
      }

      // Sphere spring
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

      // Same category attraction / close repulsion
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

      // Copy into distributed state
      state().timer = timer;
      state().count = position.size();

      for (int i = 0; i < position.size() && i < MAX_HEADLINES; i++) {
        state().positions[i] = position[i];
      }

      for (auto &f : force) {
        f.set(0);
      }
    }
  }

  void buildLineMeshFromState() {
    lineMesh.reset();
    lineMesh.primitive(Mesh::LINES);

    double drawTimer = state().timer;

    if (drawTimer <= 12.0) return;

    float t = (drawTimer - 12.0) / 3.0;

    if (t < 0) t = 0;
    if (t > 1) t = 1;

    float easeT = t * t * (3 - 2 * t);

    int count = state().count;

    for (int i = 0; i < count; i++) {
      for (int j = i + 1; j < count; j++) {
        if (categories[i] == categories[j]) {
          Vec3f start = state().positions[i];
          Vec3f end = state().positions[j];

          Vec3f animatedEnd = start + (end - start) * easeT;

          Color lineColor = Color(1, 1, 1, alphaLines);

          if (categories[i] == "WSJ") {
            lineColor = Color(1, 0.5, 0.5, alphaLines);
          }

          if (categories[i] == "FTTimes" || categories[i] == "FTT") {
            lineColor = Color(1.0, 0.694, 0.078, alphaLines);
          }

          lineMesh.vertex(start);
          lineMesh.color(lineColor);

          lineMesh.vertex(animatedEnd);
          lineMesh.color(lineColor);
        }
      }
    }
  }

  void onDraw(Graphics &g) override {
    g.clear(0.3);

    g.blending(true);
    g.blendTrans();
    g.depthTesting(true);

    buildLineMeshFromState();

    g.meshColor();
    g.lineWidth(lineWidth);
    g.draw(lineMesh);

    double drawTimer = state().timer;
    int headlineCount = state().count;

    for (int i = 0; i < textMeshes.size() && i < headlineCount; i++) {
      float t = (drawTimer - 3.0) / 2.0;

      if (t < 0) t = 0;
      if (t > 1) t = 1;

      float easeB = t * t * (3 - 2 * t);

      Vec3f start(0, 0, 0);
      Vec3f end = state().positions[i];
      Vec3f animatedPos = start + (end - start) * easeB;

      g.pushMatrix();

      g.translate(animatedPos);

      Quatd rotation = Quatd::getBillboardRotation(
          -end / end.mag(),
          Vec3d(0, 1, 0));

      g.rotate(rotation);

      // Box
      if (drawTimer > 6.0) {
        Mesh outline;
        outline.primitive(Mesh::LINE_LOOP);

        float w = boxWidth;
        float h = boxHeight;

        Color boxColor = categoryColor(categories[i]);

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

      // Text
      g.texture();
      font.tex.bind();

      g.scale(pointSize / 5.0);

    if (drawTimer > 3.0) {
  // g.color(1, 1, 1);
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