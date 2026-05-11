#include "al/app/al_App.hpp"
#include "al/graphics/al_Font.hpp"
#include <vector>

using namespace al;
using namespace std;

struct MyApp : public App {
  Font font;

  // array of text strings
  vector<string> words = {
    "HON",
    "NNN",
    "NN",
    "NNN"
  };

  // create the meshes for mapping each word
  vector<Mesh> meshes;

  void onCreate() override {
    nav().pos(0, 0, 4);
    nav().setHome();

    font.load("data/VeraMono.ttf", 100, 1024);
    font.alignCenter();

    // create a mesh for each word
    for (int i = 0; i < words.size(); i++) {
      Mesh m;
      font.write(m, words[i].c_str(), 0.2f);
      meshes.push_back(m);
    }
  }

  void onDraw(Graphics& g) override {
    g.clear(0.5, 0.5, 0.5);

    g.blending(true);
    g.blendTrans();

    g.texture();
    font.tex.bind();

    // draw each word in a different position
    for (int i = 0; i < meshes.size(); i++) {
      g.pushMatrix();

      // move each word so they don’t overlap
      g.translate(0, i * 0.5 - 1.0, 0);

      g.draw(meshes[i]);

      g.popMatrix();
    }

    font.tex.unbind();
  }
};

int main() {
  MyApp app;
  app.start();
}