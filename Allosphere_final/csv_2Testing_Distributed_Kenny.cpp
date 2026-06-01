// Core AlloLib app and distributed app headers
#include "al/app/al_App.hpp"
#include "al/app/al_GUIDomain.hpp"
#include "al/app/al_DistributedApp.hpp"

// Font, random, file/search path, presets, state distribution, and sound
#include "al/graphics/al_Font.hpp"
#include "al/math/al_Random.hpp"
#include "al/io/al_File.hpp"
#include "al_ext/statedistribution/al_CuttleboneStateSimulationDomain.hpp"
#include "al/ui/al_PresetHandler.hpp"
#include "al/ui/al_PresetServer.hpp"
#include "al/ui/al_SequenceRecorder.hpp"
#include "al/sound/al_SoundFile.hpp"

#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

using namespace al;
using namespace std;

// Maximum number of headlines allowed in distributed state.
// Fixed-size arrays are safer for distributed state than vectors.
const int MAX_HEADLINES = 200;

// Creates a random 3D vector, scaled by the input amount.
Vec3f randomVec3f(float scale) {
  return Vec3f(rnd::uniformS(), rnd::uniformS(), rnd::uniformS()) * scale;
}

// This is the shared distributed state.
// These values are sent from the primary machine to the render machines.
struct WorldState {
  double timer;                         // global app timer
  int count;                            // number of active headlines
  Vec3f positions[MAX_HEADLINES];       // headline positions
};

// Main app.
// DistributedAppWithState means this app shares WorldState across machines.
struct AlloApp : DistributedAppWithState<WorldState> {

  // GUI / preset parameters
  Parameter pointSize{"/pointSize", "", 200.0, 5.0, 600.0};
  Parameter timeStep{"/timeStep", "", 0.02, 0.01, 0.6};
  Parameter dragforce{"/dragforce", "", 0.5, 0.0, 1.0};
  Parameter stiffness{"/stiffness", "", 1.0, 0.1, 5.0};
  Parameter sphereRadius{"/sphereRadius", "", 20.0, 5.0, 40.0};
  Parameter attraction{"/attraction", "", 0.05, 0.0, 0.2};
  Parameter minDistance{"/minDistance", "", 5.0, 0.1, 50.0};
  Parameter repelStrength{"/repelStrength", "", 1.0, 0.0, 20.0};
  Parameter lineWidth{"/lineWidth", "", 4.0, 1.0, 10.0};
  Parameter boxWidth{"/boxWidth", "", 5.0, 0.1, 10.0};
  Parameter boxHeight{"/boxHeight", "", 0.5, 0.1, 3.0};
  Parameter alphaLines{"/alphaLines", "", 0.5, 0.1, 1.0};

  // Allows preset morphing to be paused/resumed.
  ParameterBool RunPresets{"/RunPresets", "", true};

  // Handles saving, recalling, and morphing presets.
  PresetHandler presets{TimeMasterMode::TIME_MASTER_FREE, "presetsGUI"};

  // Makes presets available over a local preset server.
  PresetServer presetServer{"127.0.0.1"};

  // Sound playback objects
  SoundFilePlayerTS player;
  vector<float> audioBuffer;
  bool audioReady = false;

  // Font and meshes
  Font font;
  Mesh mesh;       // stores the headline positions
  Mesh lineMesh;   // stores connection lines between headlines

  // Finds files from app/data folders.
  SearchPaths searchPaths;

  // appTimer never resets. Used for drawing timing.
  double appTimer = 0.0;

  // presetTimer resets every time a new preset is triggered.
  double presetTimer = 0.0;

  // Which preset is currently next in the sequence.
  int currentPreset = 0;

  // Time between preset changes.
  vector<double> presetTimes = {
      0.0,
      7.0,
      30.0,
      15.0
  };

  // Order that category connection lines appear.
  vector<string> lineCategoryOrder = {
      "Intercept",
      "FTT",
      "NYT",
      "DropSite",
      "WSJ"
  };

  // Subject/category label data
  vector<string> subjectNames;
  vector<Mesh> subjectMeshes;

  // Physics data
  vector<Vec3f> velocity;
  vector<Vec3f> force;
  vector<float> mass;

  // CSV data
  vector<string> words;
  vector<string> categories;
  vector<Mesh> textMeshes;

  // Loads two columns from the CSV:
  // column 1 = headline/title
  // column 2 = category/source/subject
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

  // Initializes distributed state and GUI.
  void onInit() override {

    // Enables Cuttlebone state distribution.
    auto cuttleboneDomain =
        CuttleboneStateSimulationDomain<WorldState>::enableCuttlebone(this);

    if (!cuttleboneDomain) {
      std::cerr << "WARNING: cuttlebone failed to start" << std::endl;
    }

    // Only the primary machine creates the GUI.
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
      gui.add(RunPresets);
    }
  }

  // Runs once at startup.
  void onCreate() override {
    cout << "APP STARTED" << endl;

    // Set up file search paths.
    searchPaths.addAppPaths();
    searchPaths.addRelativePath("../data");

    // Load sound file.
    string soundPath = searchPaths.find("count.wav").filepath();

    if (!player.open(soundPath.c_str())) {
      cout << "Could not open sound file: " << soundPath << endl;
    } else {
      player.setPlay();
      audioReady = true;
    }

    // Load font.
    font.load(searchPaths.find("Datatype_Condensed-Black.ttf")
                  .filepath()
                  .c_str(),
              200, 1024);

    font.alignCenter();

    // Register parameters with the preset system.
    presets << pointSize << sphereRadius << minDistance << repelStrength
            << alphaLines;

    presets.setSubDirectory("bank1");

    // Preset morph duration.
    presets.setMorphTime(3.0);

    // Step size for preset morphing.
    presets.setMorphStepTime(1.0 / graphicsDomain()->fps());

    // Start preset server.
    presetServer << presets;
    presetServer.addListener("127.0.0.1", 13560);

    // Load headline CSV.
    loadCSV(searchPaths.find("test5.csv").filepath());

    if (words.size() == 0) {
      words.push_back("NO CSV");
      categories.push_back("none");
    }

    mesh.primitive(Mesh::POINTS);
    lineMesh.primitive(Mesh::LINES);

    // Create headline positions and text meshes.
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

    // Create one subject label mesh per unique category.
    for (int i = 0; i < categories.size(); i++) {
      bool alreadyAdded = false;

      for (int j = 0; j < subjectNames.size(); j++) {
        if (categories[i] == subjectNames[j]) {
          alreadyAdded = true;
        }
      }

      if (!alreadyAdded) {
        subjectNames.push_back(categories[i]);

        Mesh subjectMesh;
        font.write(subjectMesh, categories[i].c_str(), 0.08f);
        subjectMeshes.push_back(subjectMesh);
      }
    }

    // Initialize distributed state.
    state().timer = 0.0;
    state().count = words.size();

    for (int i = 0; i < words.size(); i++) {
      state().positions[i] = mesh.vertices()[i];
    }

    nav().pos(0, 0, 40);

    // Expose parameters through parameter server.
    parameterServer() << pointSize << boxWidth << boxHeight << lineWidth
                      << alphaLines;
  }

  // Returns a color based on category.
  Color categoryColor(string c) {
    if (c == "Intercept") return Color(1, 1, 1);
    if (c == "FTT") return Color(1, 1, 1);
    if (c == "NYT") return Color(1, 1, 0);
    if (c == "DropSite") return Color(1, 0.5, 1);
    if (c == "WSJ") return Color(0, 0.5, 1);

    return Color(1, 1, 1);
  }

  // Audio callback.
  void onSound(AudioIOData &io) override {

    // If sound did not load, output silence.
    if (!audioReady) {
      while (io()) {
        io.out(0) = 0;
        io.out(1) = 0;
      }
      return;
    }

    int frames = io.framesPerBuffer();
    int channels = player.soundFile.channels;

    audioBuffer.resize(frames * channels);

    player.getFrames(frames,
                     audioBuffer.data(),
                     audioBuffer.size());

    while (io()) {
      int i = io.frame() * channels;

      io.out(0) = audioBuffer[i];

      if (channels > 1) {
        io.out(1) = audioBuffer[i + 1];
      } else {
        io.out(1) = audioBuffer[i];
      }
    }
  }

  // Physics and preset animation.
  void onAnimate(double dt) override {
    vector<Vec3f> &position = mesh.vertices();

    // Only primary machine runs simulation.
    if (isPrimary()) {
      appTimer += dt;
      presetTimer += dt;

      // Trigger presets in sequence.
      if (presetTimer > presetTimes[currentPreset]) {
        if (currentPreset == 0) {
          presets.recallPreset("preset1");
          cout << "preset1" << endl;
        } else if (currentPreset == 1) {
          presets.recallPreset("preset2");
          cout << "preset2" << endl;
        } else if (currentPreset == 2) {
          presets.recallPreset("preset3");
          cout << "preset3" << endl;
        }

        presetTimer = 0.0;
        currentPreset++;

        if (currentPreset >= presetTimes.size()) {
          currentPreset = 0;
        }
      }

      // Step preset morphing each frame.
      if (RunPresets.get()) {
        presets.stepMorphing();
      }

      float floorY = -sphereRadius * 0.5;

      // Different categories repel each other.
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

      // Push headlines away from the bottom/south pole.
      for (int i = 0; i < position.size(); i++) {
        if (position[i].y < floorY) {
          force[i] += Vec3f(0, 5.0, 0);
        }
      }

      // Keep headlines near the sphere surface.
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

      // Same categories attract, but repel if too close.
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

      // Drag slows movement.
      for (int i = 0; i < velocity.size(); i++) {
        force[i] += -velocity[i] * dragforce;
      }

      // Update velocity and position.
      for (int i = 0; i < velocity.size(); i++) {
        velocity[i] += force[i] / mass[i] * timeStep;
        position[i] += velocity[i] * timeStep;
      }

      // Send positions and timer to distributed state.
      state().timer = appTimer;
      state().count = position.size();

      for (int i = 0; i < position.size() && i < MAX_HEADLINES; i++) {
        state().positions[i] = position[i];
      }

      // Clear force accumulator.
      for (auto &f : force) {
        f.set(0);
      }
    }
  }

  // Builds category connection lines.
  // Lines appear category-by-category, with easing.
  void buildLineMeshFromState() {
    lineMesh.reset();
    lineMesh.primitive(Mesh::LINES);

    double drawTimer = state().timer;

    if (drawTimer <= 12.0) return;

    double categoryDelay = 3.0;
    double growTime = 3.0;

    int count = state().count;

    for (int c = 0; c < lineCategoryOrder.size(); c++) {
      string activeCategory = lineCategoryOrder[c];

      double categoryStart = 12.0 + c * categoryDelay;

      if (drawTimer < categoryStart) {
        continue;
      }

      float t = (drawTimer - categoryStart) / growTime;

      if (t < 0) t = 0;
      if (t > 1) t = 1;

      float easeT = t * t * (3 - 2 * t);

      Color lineColor = Color(1, 1, 1, alphaLines);

      if (activeCategory == "WSJ") {
        lineColor = Color(1, 0.5, 0.5, alphaLines);
      }

      if (activeCategory == "FTTimes" || activeCategory == "FTT") {
        lineColor = Color(1.0, 0.694, 0.078, alphaLines);
      }

      if (activeCategory == "NYT") {
        lineColor = Color(1, 1, 0, alphaLines);
      }

      if (activeCategory == "DropSite") {
        lineColor = Color(1, 0.5, 1, alphaLines);
      }

      for (int i = 0; i < count; i++) {
        for (int j = i + 1; j < count; j++) {
          if (categories[i] == activeCategory &&
              categories[j] == activeCategory) {

            Vec3f start = state().positions[i];
            Vec3f end = state().positions[j];

            Vec3f animatedEnd = start + (end - start) * easeT;

            lineMesh.vertex(start);
            lineMesh.color(lineColor);

            lineMesh.vertex(animatedEnd);
            lineMesh.color(lineColor);
          }
        }
      }
    }
  }

  // Draw callback.
  void onDraw(Graphics &g) override {
    g.clear(0.3);

    g.blending(true);
    g.blendTrans();
    g.depthTesting(true);

    // Draw connection lines.
    buildLineMeshFromState();

    g.meshColor();
    g.lineWidth(lineWidth);
    g.draw(lineMesh);

    // Draw large subject labels after 25 seconds.
    if (state().timer > 25.0) {
      for (int s = 0; s < subjectNames.size(); s++) {
        Vec3f center(0, 0, 0);
        int count = 0;

        // Find center of each category cluster.
        for (int i = 0; i < state().count; i++) {
          if (categories[i] == subjectNames[s]) {
            center += state().positions[i];
            count++;
          }
        }

        if (count > 0) {
          center /= count;

          Vec3f labelPos = center;
          labelPos.normalize(labelPos.mag() + 4.0);

          g.pushMatrix();
          g.translate(labelPos);

          Quatd rotation = Quatd::getBillboardRotation(
              -labelPos / labelPos.mag(),
              Vec3d(0, 1, 0));

          g.rotate(rotation);

          g.texture();
          font.tex.bind();

          g.scale(50.0);
          g.draw(subjectMeshes[s]);

          font.tex.unbind();
          g.popMatrix();
        }
      }
    }

    int headlineCount = state().count;

    // Draw each headline.
    for (int i = 0; i < textMeshes.size() && i < headlineCount; i++) {
      Vec3f animatedPos = state().positions[i];

      g.pushMatrix();

      g.translate(animatedPos);

      // Make text face the viewer.
      if (animatedPos.mag() > 0.0001) {
        Quatd rotation = Quatd::getBillboardRotation(
            -animatedPos / animatedPos.mag(),
            Vec3d(0, 1, 0));

        g.rotate(rotation);
      }

      g.texture();
      font.tex.bind();

      g.scale(pointSize / 5.0);
      g.draw(textMeshes[i]);

      font.tex.unbind();

      g.popMatrix();
    }
  }

  // Keyboard control for saving and recalling presets.
  bool onKeyDown(const Keyboard &k) override {
    if (k.shift()) {
      switch (k.key()) {
        case '1':
          presets.storePreset("preset1");
          cout << "Preset 1 stored." << endl;
          break;

        case '2':
          presets.storePreset("preset2");
          cout << "Preset 2 stored." << endl;
          break;

        case '3':
          presets.storePreset("preset3");
          cout << "Preset 3 stored." << endl;
          break;

        case '4':
          presets.storePreset("preset4");
          cout << "Preset 4 stored." << endl;
          break;
      }
    } else {
      switch (k.key()) {
        case '1':
          presets.recallPreset("preset1");
          cout << "Preset 1 morphing." << endl;
          break;

        case '2':
          presets.recallPreset("preset2");
          cout << "Preset 2 morphing." << endl;
          break;

        case '3':
          presets.recallPreset("preset3");
          cout << "Preset 3 morphing." << endl;
          break;

        case '4':
          presets.recallPreset("preset4");
          cout << "Preset 4 morphing." << endl;
          break;
      }
    }

    return true;
  }
};

// Main entry point.
int main() {
  AlloApp app;

  app.title("Presets");

  // Enable audio output.
  app.configureAudio(
      44100,  // sample rate
      512,    // buffer size
      2,      // output channels
      0);     // input channels

  app.start();
  return 0;
}