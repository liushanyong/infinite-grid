#include "DocumentStore.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    std::vector<std::string> splitFields(const std::string& line)
    {
        std::vector<std::string> fields;
        std::string field;
        std::istringstream stream(line);
        while (std::getline(stream, field, '\t'))
        {
            fields.push_back(field);
        }
        return fields;
    }

    bool parseBool(const std::string& text, bool& value)
    {
        if (text == "1" || text == "true")
        {
            value = true;
            return true;
        }
        if (text == "0" || text == "false")
        {
            value = false;
            return true;
        }
        return false;
    }

    bool parseFloatValue(const std::string& text, float& value)
    {
        try
        {
            size_t consumed = 0;
            value = std::stof(text, &consumed);
            return consumed == text.size();
        }
        catch (...)
        {
            return false;
        }
    }

    bool parseIntValue(const std::string& text, int& value)
    {
        try
        {
            size_t consumed = 0;
            value = std::stoi(text, &consumed);
            return consumed == text.size();
        }
        catch (...)
        {
            return false;
        }
    }
}

DocumentMemento DocumentStore::capture() const
{
    DocumentMemento memento;
    memento.objects = objects_;
    memento.layers = layers_;
    memento.currentLayer = currentLayer_;
    memento.activeTool = activeTool_;
    memento.nextObjectId = nextObjectId_;
    memento.selectedObjectId = selectedObjectId_;
    memento.selectedObjectIds = selectedObjectIds_;
    memento.hasPendingDrawPoint = hasPendingDrawPoint_;
    memento.pendingDrawX = pendingDrawX_;
    memento.pendingDrawY = pendingDrawY_;
    return memento;
}

void DocumentStore::restore(const DocumentMemento& memento)
{
    objects_ = memento.objects;
    layers_ = memento.layers;
    currentLayer_ = memento.currentLayer;
    activeTool_ = memento.activeTool;
    nextObjectId_ = memento.nextObjectId;
    selectedObjectId_ = memento.selectedObjectId;
    selectedObjectIds_ = memento.selectedObjectIds;
    hasPendingDrawPoint_ = memento.hasPendingDrawPoint;
    pendingDrawX_ = memento.pendingDrawX;
    pendingDrawY_ = memento.pendingDrawY;
}

std::string DocumentStore::serialize() const
{
    std::ostringstream out;
    out << std::setprecision(9);
    out << "OpenCADImGuiPortDocument\t1\n";
    out << "version\t1\n";
    out << "currentLayer\t" << currentLayer_ << "\n";
    out << "activeTool\t" << activeTool_ << "\n";
    out << "pending\t" << (hasPendingDrawPoint_ ? 1 : 0)
        << '\t' << pendingDrawX_ << '\t' << pendingDrawY_ << "\n";
    out << "nextObjectId\t" << nextObjectId_ << "\n";
    out << "selection\t" << selectedObjectId_ << "\n";
    out << "selections\t";
    for (size_t i = 0; i < selectedObjectIds_.size(); ++i)
    {
        if (i != 0)
        {
            out << ';';
        }
        out << selectedObjectIds_[i];
    }
    out << "\n";

    for (const LayerInfo& layer : layers_)
    {
        out << "layer\t" << layer.name
            << '\t' << (layer.visible ? 1 : 0)
            << '\t' << (layer.locked ? 1 : 0) << "\n";
    }

    for (const SceneObject& object : objects_)
    {
        out << "object\t" << object.id
            << '\t' << object.type
            << '\t' << object.name
            << '\t' << object.layer
            << '\t' << object.x1
            << '\t' << object.y1
            << '\t' << object.x2
            << '\t' << object.y2
            << '\t' << object.radius << "\n";
    }

    return out.str();
}

bool DocumentStore::loadFromText(const std::string& text, std::string& error)
{
    std::istringstream input(text);
    std::string line;
    if (!std::getline(input, line))
    {
        error = "Empty document";
        return false;
    }

    const std::vector<std::string> header = splitFields(line);
    if (header.size() != 2 || header[0] != "OpenCADImGuiPortDocument" || header[1] != "1")
    {
        error = "Unsupported document header";
        return false;
    }

    std::vector<SceneObject> loadedObjects;
    std::vector<LayerInfo> loadedLayers;
    std::string loadedCurrentLayer = "0";
    std::string loadedActiveTool = "Select";
    int loadedNextObjectId = 1;
    int loadedSelectionId = -1;
    std::vector<int> loadedSelectionIds;
    bool sawSelectionIds = false;
    bool loadedPending = false;
    float pendingX = 0.0f;
    float pendingY = 0.0f;
    bool sawPending = false;
    bool sawNextObjectId = false;
    bool sawSelection = false;

    while (std::getline(input, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        if (line.empty())
        {
            continue;
        }

        const std::vector<std::string> fields = splitFields(line);
        if (fields.empty())
        {
            continue;
        }

        if (fields[0] == "currentLayer" && fields.size() == 2)
        {
            loadedCurrentLayer = fields[1];
        }
        else if (fields[0] == "activeTool" && fields.size() == 2)
        {
            loadedActiveTool = fields[1];
        }
        else if (fields[0] == "pending" && fields.size() == 4)
        {
            float x = 0.0f;
            float y = 0.0f;
            if (!parseBool(fields[1], loadedPending)
                || !parseFloatValue(fields[2], x)
                || !parseFloatValue(fields[3], y))
            {
                error = "Invalid pending point";
                return false;
            }
            pendingX = x;
            pendingY = y;
            sawPending = true;
        }
        else if (fields[0] == "nextObjectId" && fields.size() == 2)
        {
            if (!parseIntValue(fields[1], loadedNextObjectId) || loadedNextObjectId < 1)
            {
                error = "Invalid next object id";
                return false;
            }
            sawNextObjectId = true;
        }
        else if (fields[0] == "selection" && fields.size() == 2)
        {
            if (!parseIntValue(fields[1], loadedSelectionId))
            {
                error = "Invalid selection id";
                return false;
            }
            sawSelection = true;
        }
        else if (fields[0] == "selections" && fields.size() == 2)
        {
            std::istringstream selectionStream(fields[1]);
            std::string selectionToken;
            loadedSelectionIds.clear();
            while (std::getline(selectionStream, selectionToken, ';'))
            {
                int selectedId = -1;
                if (selectionToken.empty() || !parseIntValue(selectionToken, selectedId))
                {
                    error = "Invalid selection set";
                    return false;
                }
                loadedSelectionIds.push_back(selectedId);
            }
            sawSelectionIds = true;
        }
        else if (fields[0] == "layer" && fields.size() == 4)
        {
            LayerInfo layer;
            layer.name = fields[1];
            if (!parseBool(fields[2], layer.visible) || !parseBool(fields[3], layer.locked) || layer.name.empty())
            {
                error = "Invalid layer record";
                return false;
            }
            loadedLayers.push_back(layer);
        }
        else if (fields[0] == "object" && fields.size() == 10)
        {
            SceneObject object;
            if (!parseIntValue(fields[1], object.id))
            {
                error = "Invalid object id";
                return false;
            }
            object.type = fields[2];
            object.name = fields[3];
            object.layer = fields[4];
            if (!parseFloatValue(fields[5], object.x1)
                || !parseFloatValue(fields[6], object.y1)
                || !parseFloatValue(fields[7], object.x2)
                || !parseFloatValue(fields[8], object.y2)
                || !parseFloatValue(fields[9], object.radius)
                || object.id < 1
                || object.type.empty()
                || object.name.empty()
                || object.layer.empty())
            {
                error = "Invalid object record";
                return false;
            }
            loadedObjects.push_back(object);
        }
    }

    if (!sawNextObjectId)
    {
        error = "Missing next object id";
        return false;
    }
    if (!sawSelection || !sawPending)
    {
        error = "Missing document state";
        return false;
    }

    for (const SceneObject& object : loadedObjects)
    {
        bool hasLayer = false;
        for (const LayerInfo& layer : loadedLayers)
        {
            hasLayer |= layer.name == object.layer;
        }
        if (!hasLayer)
        {
            error = "Object references unknown layer: " + object.layer;
            return false;
        }
    }

    std::vector<int> validatedSelectionIds;
    if (sawSelectionIds)
    {
        for (const int selectedId : loadedSelectionIds)
        {
            bool objectExists = false;
            for (const SceneObject& object : loadedObjects)
            {
                objectExists |= object.id == selectedId;
            }
            if (objectExists
                && std::find(validatedSelectionIds.begin(), validatedSelectionIds.end(), selectedId) == validatedSelectionIds.end())
            {
                validatedSelectionIds.push_back(selectedId);
            }
        }
    }
    else
    {
        for (const SceneObject& object : loadedObjects)
        {
            if (object.id == loadedSelectionId)
            {
                validatedSelectionIds.push_back(loadedSelectionId);
                break;
            }
        }
    }

    int primarySelectionId = loadedSelectionId;
    bool primaryIsValid = false;
    for (const SceneObject& object : loadedObjects)
    {
        primaryIsValid |= object.id == primarySelectionId;
    }
    if (!primaryIsValid)
    {
        primarySelectionId = validatedSelectionIds.empty() ? -1 : validatedSelectionIds.back();
    }
    if (std::find(validatedSelectionIds.begin(), validatedSelectionIds.end(), primarySelectionId) == validatedSelectionIds.end())
    {
        primarySelectionId = validatedSelectionIds.empty() ? -1 : validatedSelectionIds.back();
    }

    objects_ = std::move(loadedObjects);
    layers_ = std::move(loadedLayers);
    currentLayer_ = loadedCurrentLayer;
    activeTool_ = loadedActiveTool;
    nextObjectId_ = loadedNextObjectId;
    selectedObjectIds_ = validatedSelectionIds;
    selectedObjectId_ = primarySelectionId;
    hasPendingDrawPoint_ = loadedPending;
    pendingDrawX_ = pendingX;
    pendingDrawY_ = pendingY;
    return true;
}

bool DocumentStore::saveToFile(const std::string& filePath, std::string& error)
{
    std::ofstream file(filePath, std::ios::trunc);
    if (!file)
    {
        error = "Unable to open file for writing: " + filePath;
        return false;
    }

    file << serialize();
    file.flush();
    if (!file)
    {
        error = "Failed while writing file: " + filePath;
        return false;
    }
    file.close();
    if (!file)
    {
        error = "Failed while closing file: " + filePath;
        return false;
    }
    return true;
}

bool DocumentStore::loadFromFile(const std::string& filePath, std::string& error)
{
    std::ifstream file(filePath);
    if (!file)
    {
        error = "Unable to open file: " + filePath;
        return false;
    }

    std::ostringstream contents;
    contents << file.rdbuf();
    return loadFromText(contents.str(), error);
}

void DocumentStore::loadDemoDrawing()
{
    objects_ = {
        {1, "Line A",      "0",          "Line",      0.0f,    0.0f, 120.0f,  80.0f, 0.0f},
        {2, "Rectangle A", "Walls",      "Rectangle", -100.0f, -60.0f, 80.0f, 40.0f, 0.0f},
        {3, "Circle A",    "Dimensions", "Circle",     0.0f,    0.0f,  0.0f,  0.0f, 60.0f}
    };
    nextObjectId_ = 4;
}

const SceneObject* DocumentStore::findObject(int objectId) const
{
    for (const auto& object : objects_)
    {
        if (object.id == objectId)
        {
            return &object;
        }
    }
    return nullptr;
}

SceneObject* DocumentStore::findObject(int objectId)
{
    for (auto& object : objects_)
    {
        if (object.id == objectId)
        {
            return &object;
        }
    }
    return nullptr;
}

bool DocumentStore::isLayerLocked(const std::string& layerName) const
{
    const LayerInfo* layer = findLayer(layerName);
    return layer && layer->locked;
}

bool DocumentStore::isLayerVisible(const std::string& layerName) const
{
    const LayerInfo* layer = findLayer(layerName);
    return !layer || layer->visible;
}

const LayerInfo* DocumentStore::findLayer(const std::string& layerName) const
{
    for (const auto& layer : layers_)
    {
        if (layer.name == layerName)
        {
            return &layer;
        }
    }
    return nullptr;
}

LayerInfo* DocumentStore::findLayer(const std::string& layerName)
{
    for (auto& layer : layers_)
    {
        if (layer.name == layerName)
        {
            return &layer;
        }
    }
    return nullptr;
}

bool DocumentStore::addLayer(const std::string& layerName)
{
    if (layerName.empty() || findLayer(layerName))
    {
        return false;
    }

    layers_.push_back({layerName, true, false});
    return true;
}

void DocumentStore::setCurrentLayer(const std::string& layerName)
{
    currentLayer_ = layerName;
}

bool DocumentStore::toggleLayerVisible(const std::string& layerName)
{
    LayerInfo* layer = findLayer(layerName);
    if (!layer)
    {
        return false;
    }

    layer->visible = !layer->visible;
    return true;
}

bool DocumentStore::toggleLayerLocked(const std::string& layerName)
{
    LayerInfo* layer = findLayer(layerName);
    if (!layer)
    {
        return false;
    }

    layer->locked = !layer->locked;
    return true;
}

void DocumentStore::setTool(const std::string& toolName)
{
    activeTool_ = toolName.empty() ? "Select" : toolName;
    clearPendingDrawPoint();
}

SceneObject& DocumentStore::addObject(
    const std::string& type,
    const std::string& layer,
    float x1,
    float y1,
    float x2,
    float y2,
    float radius
)
{
    SceneObject object;
    object.id = nextObjectId_++;
    object.name = type + " " + std::to_string(object.id);
    object.type = type;
    object.layer = layer.empty() ? currentLayer_ : layer;
    object.x1 = x1;
    object.y1 = y1;
    object.x2 = x2;
    object.y2 = y2;
    object.radius = radius;

    objects_.push_back(object);
    return objects_.back();
}

bool DocumentStore::updateObject(
    int objectId,
    float x1,
    float y1,
    float x2,
    float y2,
    float radius
)
{
    SceneObject* object = findObject(objectId);
    if (!object)
    {
        return false;
    }

    object->x1 = x1;
    object->y1 = y1;
    object->x2 = x2;
    object->y2 = y2;
    object->radius = radius;
    return true;
}

bool DocumentStore::deleteObject(int objectId)
{
    for (auto it = objects_.begin(); it != objects_.end(); ++it)
    {
        if (it->id == objectId)
        {
            objects_.erase(it);
            selectedObjectIds_.erase(
                std::remove(selectedObjectIds_.begin(), selectedObjectIds_.end(), objectId),
                selectedObjectIds_.end()
            );
            if (selectedObjectId_ == objectId)
            {
                selectedObjectId_ = selectedObjectIds_.empty() ? -1 : selectedObjectIds_.back();
            }
            return true;
        }
    }
    return false;
}

void DocumentStore::selectObject(int objectId, SelectionModifier modifier)
{
    if (modifier == SelectionModifier::Replace)
    {
        selectedObjectIds_.clear();
        selectedObjectId_ = -1;
        if (objectId >= 0 && findObject(objectId))
        {
            selectedObjectIds_.push_back(objectId);
            selectedObjectId_ = objectId;
        }
        return;
    }

    if (objectId < 0)
    {
        selectedObjectIds_.clear();
        selectedObjectId_ = -1;
        return;
    }

    if (findObject(objectId) == nullptr)
    {
        return;
    }

    const auto existing = std::find(selectedObjectIds_.begin(), selectedObjectIds_.end(), objectId);
    if (existing != selectedObjectIds_.end())
    {
        selectedObjectIds_.erase(existing);
        selectedObjectId_ = selectedObjectIds_.empty() ? -1 : selectedObjectIds_.back();
        return;
    }

    selectedObjectIds_.push_back(objectId);
    selectedObjectId_ = objectId;
}

void DocumentStore::setSelection(const std::vector<int>& objectIds)
{
    selectedObjectIds_.clear();
    selectedObjectId_ = -1;

    for (const int objectId : objectIds)
    {
        if (findObject(objectId)
            && std::find(selectedObjectIds_.begin(), selectedObjectIds_.end(), objectId) == selectedObjectIds_.end())
        {
            selectedObjectIds_.push_back(objectId);
        }
    }

    selectedObjectId_ = selectedObjectIds_.empty() ? -1 : selectedObjectIds_.back();
}

void DocumentStore::setPendingDrawPoint(float x, float y)
{
    hasPendingDrawPoint_ = true;
    pendingDrawX_ = x;
    pendingDrawY_ = y;
}

void DocumentStore::clearPendingDrawPoint()
{
    hasPendingDrawPoint_ = false;
    pendingDrawX_ = 0.0f;
    pendingDrawY_ = 0.0f;
}
