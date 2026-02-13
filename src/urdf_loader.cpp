#include "urdf_loader.h"
#include "components/URDFLink.h"
#include "components/URDFAsset.h"
#include "model.h"
#include <filesystem>
#include <cstring>
#include <sstream>
#include <tinyxml2.h>
#include <unordered_map>
#include <unordered_set>

using tinyxml2::XMLElement;
using tinyxml2::XMLDocument;
using tinyxml2::XML_SUCCESS;

static void
logURDFLoad(const std::string& message)
{
  FILE* logFile = std::fopen("/tmp/matrix-urdf.log", "a");
  if (!logFile) {
    return;
  }
  std::fprintf(logFile, "%s\n", message.c_str());
  std::fflush(logFile);
  std::fclose(logFile);
}

static bool
parseVec3(const char* text, glm::vec3& out)
{
  if (!text) {
    return false;
  }
  std::stringstream ss(text);
  ss >> out.x >> out.y >> out.z;
  return !ss.fail();
}

static std::string
resolveMeshPath(const std::filesystem::path& urdfPath,
                const std::string& meshPath)
{
  if (meshPath.empty()) {
    return "";
  }
  if (meshPath.rfind("package://", 0) == 0) {
    std::filesystem::path stripped = meshPath.substr(strlen("package://"));
    return (urdfPath.parent_path() / stripped).string();
  }

  std::filesystem::path path(meshPath);
  if (path.is_absolute()) {
    return path.string();
  }
  return (urdfPath.parent_path() / path).string();
}

static const char*
childAttribute(XMLElement* parent, const char* childName, const char* attrName)
{
  if (!parent) {
    return nullptr;
  }
  auto* child = parent->FirstChildElement(childName);
  if (!child) {
    return nullptr;
  }
  return child->Attribute(attrName);
}

bool
loadURDFFromFile(std::shared_ptr<EntityRegistry> registry,
                 const std::string& urdfPath,
                 const glm::vec3& basePosition,
                 URDFLoadResult& outResult,
                 std::string& outError)
{
  logURDFLoad("URDF load: " + urdfPath);
  XMLDocument doc;
  if (doc.LoadFile(urdfPath.c_str()) != XML_SUCCESS) {
    outError = "Failed to load URDF: " + urdfPath;
    logURDFLoad(outError);
    return false;
  }

  XMLElement* robot = doc.FirstChildElement("robot");
  if (!robot) {
    outError = "Missing <robot> root in URDF.";
    logURDFLoad(outError);
    return false;
  }

  std::filesystem::path urdfFsPath(urdfPath);
  std::unordered_map<std::string, entt::entity> linkEntities;
  std::unordered_map<std::string, std::string> linkMeshes;
  std::unordered_map<std::string, float> linkScales;
  std::unordered_map<std::string, glm::vec3> linkVisualOrigins;
  std::unordered_map<std::string, glm::vec3> linkVisualOriginRpy;
  std::unordered_set<std::string> childLinks;

  for (auto* link = robot->FirstChildElement("link"); link;
       link = link->NextSiblingElement("link")) {
    const char* linkName = link->Attribute("name");
    if (!linkName) {
      continue;
    }

    auto* visual = link->FirstChildElement("visual");
    auto* geometry = visual ? visual->FirstChildElement("geometry") : nullptr;
    const char* meshFilename = childAttribute(geometry, "mesh", "filename");
    if (meshFilename) {
      std::string resolved = resolveMeshPath(urdfFsPath, meshFilename);
      linkMeshes[linkName] = resolved;
      logURDFLoad(std::string("URDF mesh: ") + linkName + " -> " + resolved);
      auto* mesh = geometry ? geometry->FirstChildElement("mesh") : nullptr;
      if (mesh) {
        glm::vec3 scaleVec(1.0f);
        if (parseVec3(mesh->Attribute("scale"), scaleVec)) {
          linkScales[linkName] = scaleVec.x;
        }
      }
    }

    // Parse visual origin if present (both position and rotation, like rviz)
    if (visual) {
      auto* origin = visual->FirstChildElement("origin");
      if (origin) {
        glm::vec3 visualOrigin(0.0f);
        parseVec3(origin->Attribute("xyz"), visualOrigin);
        linkVisualOrigins[linkName] = visualOrigin;
        
        glm::vec3 visualOriginRpy(0.0f);
        parseVec3(origin->Attribute("rpy"), visualOriginRpy);
        linkVisualOriginRpy[linkName] = visualOriginRpy;
      }
    }

    entt::entity entity = registry->createPersistent();
    linkEntities[linkName] = entity;
    outResult.entities.push_back(entity);

    float scale = 1.0f;
    auto scaleIt = linkScales.find(linkName);
    if (scaleIt != linkScales.end()) {
      scale = scaleIt->second;
    }

    glm::vec3 visualOrigin(0.0f);
    auto originIt = linkVisualOrigins.find(linkName);
    if (originIt != linkVisualOrigins.end()) {
      // Use URDF values directly - no coordinate conversion (following rviz pattern)
      visualOrigin = originIt->second;
    }

    registry->emplace<Positionable>(entity,
                                    glm::vec3(0.0f),
                                    visualOrigin,
                                    glm::vec3(0.0f),
                                    scale);
  }

  if (linkEntities.empty()) {
    outError = "No <link> elements found in URDF.";
    logURDFLoad(outError);
    return false;
  }

  for (auto* joint = robot->FirstChildElement("joint"); joint;
       joint = joint->NextSiblingElement("joint")) {
    const char* type = joint->Attribute("type");
    const char* parentName = childAttribute(joint, "parent", "link");
    const char* childName = childAttribute(joint, "child", "link");
    if (!type || !parentName || !childName) {
      continue;
    }

    auto parentIt = linkEntities.find(parentName);
    auto childIt = linkEntities.find(childName);
    if (parentIt == linkEntities.end() || childIt == linkEntities.end()) {
      continue;
    }

    childLinks.insert(childName);

    URDFLink link;
    link.parent = parentIt->second;
    link.jointType = URDFLink::JointType::Fixed;
    if (std::string(type) == "revolute" || std::string(type) == "continuous") {
      link.jointType = URDFLink::JointType::Revolute;
    } else if (std::string(type) == "prismatic") {
      link.jointType = URDFLink::JointType::Prismatic;
    }

    auto* origin = joint->FirstChildElement("origin");
    if (origin) {
      parseVec3(origin->Attribute("xyz"), link.originPos);
      parseVec3(origin->Attribute("rpy"), link.originRpy);
    }

    auto* axis = joint->FirstChildElement("axis");
    if (axis) {
      parseVec3(axis->Attribute("xyz"), link.axis);
    }

    registry->emplace<URDFLink>(childIt->second, link);
    
    // Store visual origin rotation if it exists (for this child link)
    auto rpyIt = linkVisualOriginRpy.find(childName);
    if (rpyIt != linkVisualOriginRpy.end()) {
      auto& storedLink = registry->get<URDFLink>(childIt->second);
      storedLink.visualOriginRpy = rpyIt->second; // Store in radians
    }
  }

  for (const auto& [linkName, entity] : linkEntities) {
    auto meshIt = linkMeshes.find(linkName);
    if (meshIt != linkMeshes.end() && !meshIt->second.empty()) {
      registry->emplace<Model>(entity, meshIt->second);
    }
  }

  // basePosition is in graphics space, convert to URDF space
  // Graphics: X=right, Y=up, Z=out of screen (positive)
  // URDF: X=backwards, Y=right, Z=up
  // Inverse of: Graphics X = URDF Y, Graphics Y = URDF Z, Graphics Z = -URDF X
  // So: URDF X = -Graphics Z, URDF Y = Graphics X, URDF Z = Graphics Y
  glm::vec3 urdfBasePos(-basePosition.z, basePosition.x, basePosition.y);

  for (const auto& [linkName, entity] : linkEntities) {
    if (childLinks.contains(linkName)) {
      continue;
    }
    // Root link: set URDF world position/rotation (all math in URDF space)
    if (registry->all_of<URDFLink>(entity)) {
      auto& link = registry->get<URDFLink>(entity);
      link.urdfWorldPos = urdfBasePos;
      link.urdfWorldRot = glm::vec3(0.0f); // Default rotation
      link.dirty = true;
    } else {
      // Create URDFLink for root if it doesn't exist
      URDFLink link;
      link.parent = entt::null;
      link.urdfWorldPos = urdfBasePos;
      link.urdfWorldRot = glm::vec3(0.0f);
      link.dirty = true;
      registry->emplace<URDFLink>(entity, link);
    }
    
    // Store visual origin rotation for root link if it exists
    auto rpyIt = linkVisualOriginRpy.find(linkName);
    if (rpyIt != linkVisualOriginRpy.end()) {
      auto& link = registry->get<URDFLink>(entity);
      link.visualOriginRpy = rpyIt->second; // Store in radians
    }
    
    registry->emplace_or_replace<URDFAsset>(entity, URDFAsset{ urdfPath });
    outResult.roots.push_back(entity);
  }

  return true;
}
