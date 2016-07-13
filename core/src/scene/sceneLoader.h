#pragma once

#include "gl/uniform.h"
#include "scene/scene.h"
#include "tangram.h"
#include <string>
#include <vector>
#include <memory>
#include <tuple>
#include <sstream>
#include <mutex>

#include "yaml-cpp/yaml.h"
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"

namespace Tangram {

class TileManager;
class SceneLayer;
class View;
class ShaderProgram;
class Material;
class Style;
struct StyleParam;
struct MaterialTexture;
class PointLight;
class DataSource;
struct Filter;
struct TextureFiltering;
struct TextureOptions;

// 0: type, 1: values
struct StyleUniform {
    std::string type;
    UniformValue value;
};

struct SceneLoader {
    using Node = YAML::Node;

    static bool loadScene(std::shared_ptr<Scene> _scene);
    static bool loadConfig(const std::string& _sceneString, Node& _root);
    static bool applyConfig(Node& config, Scene& scene);
    static void applyUpdates(Node& root, const std::vector<SceneUpdate>& updates);
    static void applyGlobalProperties(Node& node, Scene& scene);

    /*** all public for testing ***/

    static void loadBackground(Node background, Scene& scene);
    static void loadSource(const std::string& name, const Node& source, const Node& sources, Scene& scene);
    static void loadSourceRasters(std::shared_ptr<DataSource>& source, Node rasterNode, const Node& sources,
                                  Scene& scene);
    static void loadTexture(const std::pair<Node, Node>& texture, Scene& scene);
    static void loadLayer(const std::pair<Node, Node>& layer, Scene& scene);
    static void loadLight(const std::pair<Node, Node>& light, Scene& scene);
    static void loadCameras(Node cameras, Scene& scene);
    static void loadCamera(const Node& camera, Scene& scene);
    static void loadStyleProps(Style& style, Node styleNode, Scene& scene);
    static void loadMaterial(Node matNode, Material& material, Scene& scene, Style& style);
    static void loadShaderConfig(Node shaders, Style& style, Scene& scene);
    static SceneLayer loadSublayer(Node layer, const std::string& name, Scene& scene);
    static Filter generateFilter(Node filter, Scene& scene);
    static Filter generateAnyFilter(Node filter, Scene& scene);
    static Filter generateNoneFilter(Node filter, Scene& scene);
    static Filter generatePredicate(Node filter, std::string _key);
    /* loads a texture with default texture properties */
    static bool loadTexture(const std::string& url, Scene& scene);
    static std::shared_ptr<Texture> fetchTexture(const std::string& name, const std::string& url,
            const TextureOptions& options, bool generateMipmaps, Scene& scene);
    static bool extractTexFiltering(Node& filtering, TextureFiltering& filter);

    /*
     * Sprite nodes are created using a default 1x1 black texture when sprite atlas is requested over the network.
     * Once a sprite atlas has been fetched, sprite nodes need to be updated according to the width/height of the
     * fetched sprite atlas.
     */
    static void updateSpriteNodes(const std::string& texName,
            std::shared_ptr<Texture>& texture, Scene& scene);

    static MaterialTexture loadMaterialTexture(Node matCompNode, Scene& scene, Style& style);

    static void parseStyleParams(Node params, Scene& scene, const std::string& propPrefix,
                                 std::vector<StyleParam>& out);
    static void parseTransition(Node params, Scene& scene, std::vector<StyleParam>& out);

    static bool parseStyleUniforms(const Node& value, Scene* scene, StyleUniform& styleUniform);

    static void parseGlobals(const Node& node, Scene& scene, const std::string& key="");
    static void parseLightPosition(Node position, PointLight& light);

    static bool loadStyle(const std::string& styleName, Node config, Scene& scene);

    static std::mutex m_textureMutex;
    SceneLoader() = delete;

};

}
