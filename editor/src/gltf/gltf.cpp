/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2022
http://trederia.blogspot.com

crogine editor - Zlib license.

This software is provided 'as-is', without any express or
implied warranty.In no event will the authors be held
liable for any damages arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute
it freely, subject to the following restrictions :

1. The origin of this software must not be misrepresented;
you must not claim that you wrote the original software.
If you use this software in a product, an acknowledgment
in the product documentation would be appreciated but
is not required.

2. Altered source versions must be plainly marked as such,
and must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any
source distribution.

-----------------------------------------------------------------------*/

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "../ModelState.hpp" //includes tinygltf
#include "../SrgbTransform.hpp"
#include "../UIConsts.hpp"

#include <crogine/detail/glm/gtc/type_ptr.hpp>
#include <crogine/detail/glm/gtc/matrix_inverse.hpp>
#include <crogine/detail/glm/gtx/quaternion.hpp>
#include <crogine/detail/glm/gtx/matrix_decompose.hpp>

#include <crogine/graphics/MeshBuilder.hpp>
#include <crogine/detail/OpenGL.hpp>
#include <crogine/gui/Gui.hpp>

#include <crogine/graphics/MeshData.hpp>

#include <crogine/util/Matrix.hpp>

#include <string>
#include <cstring>

namespace
{
    struct AnimSampler final
    {
        std::string interpType;
        std::vector<float> inputs;
        std::vector<glm::vec4> outputs;
    };

    struct AnimChannel final
    {
        std::string path;
        std::int32_t samplerIndex = -1;
        std::int32_t node = -1;
    };

    struct Anim final
    {
        std::string name;
        std::vector<AnimSampler> samplers;
        std::vector<AnimChannel> channels;
        float start = std::numeric_limits<float>::max();
        float end = std::numeric_limits<float>::min();
        float currentTime = 0.f;
    };

    //TODO this should be a member really
    std::vector<Anim> animations;
    bool convertVertexColourspace = true;
    bool skipSecondUV = true;
    float SampleRate = 15.f;
}

void ModelState::showGLTFBrowser()
{
    std::int32_t ID = 2000;
    const std::function<void(std::int32_t, const std::vector<tf::Node>&)> drawNode
        = [&](std::int32_t idx, const std::vector<tf::Node>& nodes)
    {
        const auto& node = nodes[idx];
        if (node.mesh > -1)
        {
            std::string label = node.name + "##";
            std::string IDString = std::to_string(ID++);
            label += IDString;

            ImGui::SetNextItemOpen(ID == 2001, ImGuiCond_Once);
            if (ImGui::TreeNode(label.c_str()))
            {
                static bool importAnim = false;
                if (node.skin > -1)
                {
                    auto boxLabel = "Import Animation##" + IDString;
                    ImGui::Checkbox(boxLabel.c_str(), &importAnim);

                    if (importAnim)
                    {
                        ImGui::PushItemWidth(100.f);
                        if (ImGui::InputFloat("Resample Rate", &SampleRate, 1.f, 5.f, "%.1f"))
                        {
                            SampleRate = std::min(60.f, std::max(2.f, SampleRate));
                        }
                        ImGui::PopItemWidth();
                        ImGui::SameLine();
                        uiConst::showToolTip("Number of keyframes per second to resample the animation.\nLower values create smaller model files but may lose detail.\n15-20 is usually reasonable.");
                    }
                    ImGui::Separator();
                }
                else
                {
                    importAnim = false;
                }
                ImGui::Checkbox("Convert Vertex Colourspace", &convertVertexColourspace);
                ImGui::SameLine();
                uiConst::showToolTip("Converts incoming vertex colour data from sRGB to linear space.\nUseful if you have data rather than colour information stored in the vertex colour channel");

                ImGui::Checkbox("Skip UV1", &skipSecondUV);
                ImGui::SameLine();
                uiConst::showToolTip("Only import the first UV channel (if it exists)");

                auto buttonLabel = "Import##" + IDString;
                if (ImGui::Button(buttonLabel.c_str()))
                {
                    //import the node
                    parseGLTFNode(idx, importAnim);
                    m_GLTFLoader.reset();
                    m_browseGLTF = false;
                    ImGui::CloseCurrentPopup();
                }

                for (const auto& child : node.children)
                {
                    drawNode(child, nodes);
                }
                ImGui::TreePop();
            }
        }
    };

    //ImGui::ShowDemoWindow();
    ImGui::OpenPopup("Scene Browser");
    if (ImGui::BeginPopupModal("Scene Browser", &m_browseGLTF, ImGuiWindowFlags_AlwaysAutoResize))
    {
        const auto& nodes = m_GLTFScene.nodes;
        for (auto i = 0u; i < nodes.size(); ++i)
        {
            drawNode(static_cast<std::int32_t>(i), nodes);
        }

        ImGui::NewLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            m_GLTFLoader.reset();
            m_browseGLTF = false;
            ImGui::CloseCurrentPopup(); 
        }
        ImGui::EndPopup();
    }
}

void ModelState::parseGLTFNode(std::int32_t idx, bool loadAnims)
{
    auto success = importGLTF(idx, loadAnims);

    //load animations if selected
    if (success && loadAnims)
    {
        parseGLTFAnimations(idx);
        parseGLTFSkin(idx, m_entities[EntityID::ActiveModel].addComponent<cro::Skeleton>());

        buildSkeleton(); //for display
    }
}

void ModelState::parseGLTFAnimations(std::int32_t idx)
{
    //pre-processes the animation data for skinning
    animations.clear();
    animations.resize(m_GLTFScene.animations.size());

    for (auto i = 0u; i < animations.size(); ++i)
    {
        const auto& glAnim = m_GLTFScene.animations[i];
        animations[i].name = glAnim.name;

        //process samplers...
        //inputs are the current global time in seconds
        //at which this key frame occurs. Outputs are the
        //channel values (TRS) at this point in time.
        animations[i].samplers.resize(glAnim.samplers.size());
        for (auto j = 0u; j < animations[i].samplers.size(); ++j)
        {
            const auto& glSampler = glAnim.samplers[j];
            auto& sampler = animations[i].samplers[j];
            sampler.interpType = glSampler.interpolation;

            //read input values from buffer
            {
                const auto& accessor = m_GLTFScene.accessors[glSampler.input];
                const auto& view = m_GLTFScene.bufferViews[accessor.bufferView];
                const auto& buffer = m_GLTFScene.buffers[view.buffer];
                auto index = accessor.byteOffset + view.byteOffset;

                for (auto k = 0u; k < accessor.count; ++k)
                {
                    float v = 0.f;
                    std::memcpy(&v, &buffer.data[index], sizeof(float));
                    sampler.inputs.push_back(v);
                    index += sizeof(float);
                }

                for (auto f : sampler.inputs)
                {
                    if (f < animations[i].start)
                    {
                        animations[i].start = f;
                    }

                    if (f > animations[i].end)
                    {
                        animations[i].end = f;
                    }
                }
            }

            //read output values from buffer
            {
                const auto& accessor = m_GLTFScene.accessors[glSampler.output];
                const auto& view = m_GLTFScene.bufferViews[accessor.bufferView];
                const auto& buffer = m_GLTFScene.buffers[view.buffer];
                auto index = accessor.byteOffset + view.byteOffset;

                switch (accessor.type)
                {
                    case TINYGLTF_TYPE_VEC3:
                    {
                        for (auto k = 0; k < accessor.count; ++k)
                        {
                            glm::vec3 v = glm::vec3(0.f);
                            std::memcpy(&v, &buffer.data[index + (k * sizeof(glm::vec3))], sizeof(glm::vec3));
                            sampler.outputs.push_back(glm::vec4(v, 0.f));
                        }
                    }
                        break;
                    case TINYGLTF_TYPE_VEC4:
                    {
                        for (auto k = 0; k < accessor.count; ++k)
                        {
                            glm::vec4 v = glm::vec4(0.f);
                            std::memcpy(&v, &buffer.data[index + (k * sizeof(glm::vec4))], sizeof(glm::vec4));
                            sampler.outputs.push_back(v);
                        }
                    }
                        break;
                    default:
                        CRO_ASSERT(false, "");
                        LogW << "Unknown accessor type found in animation sampler for " << animations[i].name << std::endl;
                        break;
                }
            }
        }


        //process channels
        animations[i].channels.resize(glAnim.channels.size());
        for (auto j = 0u; j < animations[i].channels.size(); ++j)
        {
            const auto& glChannel = glAnim.channels[j];
            auto& channel = animations[i].channels[j];
            channel.path = glChannel.target_path;
            channel.node = glChannel.target_node;
            channel.samplerIndex = glChannel.sampler;
        }
    }
}

void ModelState::parseGLTFSkin(std::int32_t idx, cro::Skeleton& dest)
{
    //TODO model might have multple skins (ie for multiple meshes)
    //so we need to traverse the tree and parse all of them.

    //fetch the skin details
    const auto& node = m_GLTFScene.nodes[idx];
    CRO_ASSERT(node.skin > -1 && node.skin < m_GLTFScene.skins.size(), "");
    const auto& skin = m_GLTFScene.skins[node.skin];

    std::vector<glm::mat4> inverseBindPose(skin.joints.size());

    const auto& accessor = m_GLTFScene.accessors[skin.inverseBindMatrices];
    const auto& bufferView = m_GLTFScene.bufferViews[accessor.bufferView];
    const auto& buffer = m_GLTFScene.buffers[bufferView.buffer];
    std::size_t index = accessor.byteOffset + bufferView.byteOffset;

    //create a vector of parent IDs as nodes only track children
    std::vector<std::int32_t> parents(m_GLTFScene.nodes.size());
    std::fill(parents.begin(), parents.end(), -1);
    for (auto i = 0u; i < m_GLTFScene.nodes.size(); ++i)
    {
        const auto& node = m_GLTFScene.nodes[i];
        for (auto j = 0u; j < node.children.size(); ++j)
        {
            parents[node.children[j]] = static_cast<std::int32_t>(i);
        }
    }

    //read the inv bindpose
    std::memcpy(inverseBindPose.data(), buffer.data.data() + index, sizeof(glm::mat4) * inverseBindPose.size());
    index += sizeof(glm::mat4) * inverseBindPose.size();

    dest.setInverseBindPose(inverseBindPose);

    //copy the nodes as we'll modify the transforms for each key frame
    auto sceneNodes = m_GLTFScene.nodes;

    //functions to process a frame
    const auto getLocalJoint = [](const tf::Node& node)
    {
        glm::mat4 jointMat = glm::mat4(1.f);
        glm::vec3 translation(0.f);
        glm::quat rotation(1.f, 0.f, 0.f, 0.f);
        glm::vec3 scale(1.f);

        if (node.translation.size() == 3)
        {
            translation.x = static_cast<float>(node.translation[0]);
            translation.y = static_cast<float>(node.translation[1]);
            translation.z = static_cast<float>(node.translation[2]);
        }
        if (node.rotation.size() == 4)
        {
            rotation.w = static_cast<float>(node.rotation[3]);
            rotation.x = static_cast<float>(node.rotation[0]);
            rotation.y = static_cast<float>(node.rotation[1]);
            rotation.z = static_cast<float>(node.rotation[2]);
        }
        if (node.scale.size() == 3)
        {
            scale.x = static_cast<float>(node.scale[0]);
            scale.y = static_cast<float>(node.scale[1]);
            scale.z = static_cast<float>(node.scale[2]);
        }

        cro::Joint j(translation, rotation, scale);
        j.worldMatrix = cro::Joint::combine(j);

        return j;
    };
    
    const auto getWorldJoint = [&](std::int32_t i)
    {
        const auto& node = sceneNodes[i];
        auto tempJoint = getLocalJoint(node);
        tempJoint.parent = parents[i];

        std::int32_t currentParent = tempJoint.parent;
        while (currentParent != -1)
        {
            auto j = getLocalJoint(sceneNodes[currentParent]);
            tempJoint.worldMatrix = j.worldMatrix * tempJoint.worldMatrix;
            currentParent = parents[currentParent];
        }

        return tempJoint;
    };

    //if the mesh has a parent node, we want to apply its world
    //transform to the joints, as it will get pruned from the tree
    //when we move nodes into our frames
    glm::mat4 parentTx(1.f);
    if (parents[idx] != -1)
    {
        parentTx = getWorldJoint(parents[idx]).worldMatrix;
    }
    dest.setRootTransform(parentTx);

    auto inverseParentTx = glm::inverse(parentTx);

    auto createFrame = [&]()
    {
        std::vector<cro::Joint> frame;
        for (auto i = 0u; i < inverseBindPose.size(); ++i)
        {
            auto& j = frame.emplace_back(getWorldJoint(skin.joints[i]));

            //remove any root transform here so it's applied during animation
            j.worldMatrix = inverseParentTx * j.worldMatrix;

            //skins are parented to a scene node, so we need to re-adjust
            //the frame's nodes parent IDs to be local to the frame.
            if (auto result = std::find(skin.joints.begin(), skin.joints.end(), parents[skin.joints[i]]); result == skin.joints.end())
            {
                j.parent = -1;
            }
            else
            {
                j.parent = static_cast<std::int32_t>(std::distance(skin.joints.begin(), result));
            }
        }
        dest.addFrame(frame);
    };

    //base frame
    if (animations.empty())
    {
        //animations need at least 2 frames
        createFrame();
        createFrame();

        //update the output so we have something to look at.
        cro::SkeletalAnim anim;
        anim.frameCount = 2;
        anim.name = "Default";
        dest.addAnimation(anim); //empty 1 frame anim
    }
    
    for (auto& anim : animations)
    {
        cro::SkeletalAnim skelAnim;
        skelAnim.name = anim.name;
        skelAnim.startFrame = static_cast<std::uint32_t>(dest.getFrameCount());
        skelAnim.currentFrame = skelAnim.startFrame;

        //glTF doesn't use a fixed frame rate, rather it
        //supports poses at certain times. Ideally we should
        //adapt cro to this method, but for now we'll assume
        //a fixed frame rate and create series of samples
        //from which keyframes can be built.

        //the main problem with this approach is that, especially
        //at low sample rates, we may miss the actual keyframes
        //stored in the file. Higher samples rates, however,
        //cost more memory.

        //static constexpr float SampleRate = 20.f;
        const float FixedStep = 1.f / SampleRate;
        skelAnim.frameRate = SampleRate;

        while(anim.currentTime < anim.end)
        {
            //for each channel
            bool addFrame = false;
            for (const auto& channel : anim.channels)
            {
                //get channel sampler
                const auto& sampler = anim.samplers[channel.samplerIndex];
                //TODO skip interpolation if type is 'STEP'
                //TODO stop ignoring cubic spline and parse properly instead of interpolating

                for (auto i = 0u; i < sampler.inputs.size() - 1; ++i)
                {
                    //if current time lies between this sampler and next sampler
                    if ((anim.currentTime >= sampler.inputs[i])
                        && (anim.currentTime <= sampler.inputs[i + 1]))
                    {
                        //calculate an interpolated time and render results into
                        //node's transforms.
                        float t = (anim.currentTime - sampler.inputs[i]) / (sampler.inputs[i + 1] - sampler.inputs[i]);

                        if (channel.path == "translation")
                        {
                            glm::vec4 translation = glm::mix(sampler.outputs[i], sampler.outputs[i + 1], t);
                            sceneNodes[channel.node].translation = { translation.x, translation.y, translation.z };
                        }
                        else if(channel.path == "rotation")
                        {
                            glm::quat a = glm::quat(1.f, 0.f, 0.f, 0.f);
                            a.x = sampler.outputs[i].x;
                            a.y = sampler.outputs[i].y;
                            a.z = sampler.outputs[i].z;
                            a.w = sampler.outputs[i].w;

                            glm::quat b = glm::quat(1.f, 0.f, 0.f, 0.f);
                            b.x = sampler.outputs[i + 1].x;
                            b.y = sampler.outputs[i + 1].y;
                            b.z = sampler.outputs[i + 1].z;
                            b.w = sampler.outputs[i + 1].w;

                            glm::quat rot = glm::normalize(glm::slerp(a, b, t));
                            sceneNodes[channel.node].rotation = { rot.x, rot.y, rot.z, rot.w };
                        }
                        else if (channel.path == "scale")
                        {
                            glm::vec4 scale = glm::mix(sampler.outputs[i], sampler.outputs[i + 1], t);
                            sceneNodes[channel.node].scale = { scale.x, scale.y, scale.z };
                        }

                        addFrame = true;
                    }
                }
            }

            if (addFrame)
            {
                createFrame();
                skelAnim.frameCount++;
            }
            anim.currentTime += FixedStep;
        }

        skelAnim.looped = true; //glTF doesn't have this property... 
        //TODO use the extras property to tag message triggers, looping etc?
        //or shall we make this part of the editor?

        //we won't get anything if the animation contains only one frame
        if (skelAnim.frameCount == 0)
        {
            createFrame();
            skelAnim.frameCount++;
        }

        dest.addAnimation(skelAnim);
    }
}

bool ModelState::importGLTF(std::int32_t idx, bool loadAnims)
{
    //traverse the node and acumulate any previous transforms in the tree

    //start with mesh. This contains a Primitive for each sub-mesh
    //which in turn contains the indices into the accessor array.

    //an accessor contains info about a buffer view, such as its index
    //in the bufferview array, and which data type the buffer contains
    //ie, vec2, vec3 etc. componentType matches the GL_ENUM for GL_FLOAT,
    //GL_UINT etc. It may contain an additional offset which should be
    //added to the beginning of the view offset, and subtracted from
    //the view's length

    //a buffer view contains the buffer index, the offset into that
    //buffer at which data starts, the length of the data, and the stride.

    std::uint16_t attribFlags = 0u;
    std::vector<float> vertices;
    std::vector<std::vector<std::uint32_t>> indices;
    std::vector<std::int32_t> primitiveTypes;
    std::size_t indexOffset = 0;

    CRO_ASSERT(m_GLTFScene.nodes[idx].mesh < m_GLTFScene.meshes.size(), "");
    const auto& mesh = m_GLTFScene.meshes[m_GLTFScene.nodes[idx].mesh];

    if (mesh.primitives.size() > cro::Mesh::IndexData::MaxBuffers)
    {
        LogE << "Model has " << mesh.primitives.size() << " sub-meshes. Maximum is 32. Model not loaded" << std::endl;
        return false;
    }

    for (const auto& prim : mesh.primitives)
    {
        std::uint16_t flags = 0;
        std::array<std::int32_t, cro::Mesh::Attribute::Total> attribIndices = {};
        std::fill(attribIndices.begin(), attribIndices.end(), -1);

        for (const auto& [name, idx] : prim.attributes)
        {
            if (name == "POSITION")
            {
                flags |= cro::VertexProperty::Position;
                attribIndices[cro::Mesh::Attribute::Position] = idx;
            }
            else if (name == "NORMAL")
            {
                flags |= cro::VertexProperty::Normal;
                attribIndices[cro::Mesh::Attribute::Normal] = idx;
            }
            else if (name == "TANGENT")
            {
                flags |= cro::VertexProperty::Tangent | cro::VertexProperty::Bitangent;
                attribIndices[cro::Mesh::Attribute::Tangent] = idx;
            }
            else if (name == "TEXCOORD_0")
            {
                flags |= cro::VertexProperty::UV0;
                attribIndices[cro::Mesh::Attribute::UV0] = idx;
            }
            else if (name == "TEXCOORD_1"
                && !skipSecondUV)
            {
                flags |= cro::VertexProperty::UV1;
                attribIndices[cro::Mesh::Attribute::UV1] = idx;
            }
            else if (name == "COLOR_0")
            {
                flags |= cro::VertexProperty::Colour;
                attribIndices[cro::Mesh::Attribute::Colour] = idx;
            }
            else if (name == "JOINTS_0"
                && loadAnims)
            {
                flags |= cro::VertexProperty::BlendIndices;
                attribIndices[cro::Mesh::Attribute::BlendIndices] = idx;
            }
            else if (name == "WEIGHTS_0"
                && loadAnims)
            {
                flags |= cro::VertexProperty::BlendWeights;
                attribIndices[cro::Mesh::Attribute::BlendWeights] = idx;
            }
        }

        if ((flags & cro::VertexProperty::Position) == 0)
        {
            LogE << "No vertex positions found. Model not loaded." << std::endl;
            return false;
        }

        if (attribFlags != 0
            && attribFlags != flags)
        {
            //TODO ideally mesh format should account for these cases
            //after all each submesh has its own shader/material aaaanyway
            //but it also means creating multiple VBOs per VAO and... meh
            LogW << "submesh with differing vertex attribs found - skipping..." << std::endl;
            continue;
        }
        attribFlags = flags;

        //interleave and concat vertex data
        //TODO this might actually already be interleaved - and so would
        //be faster if we know this and do a straight copy...
        std::array<std::pair<std::vector<float>, std::size_t>, cro::Mesh::Attribute::Total> tempData; //float components, component size
        auto getComponentCount = [](int type)
        {
            switch (type)
            {
            default: return 0;
            case TINYGLTF_TYPE_SCALAR:
                return 1;
            case TINYGLTF_TYPE_VEC2:
                return 2;
            case TINYGLTF_TYPE_VEC3:
                return 3;
            case TINYGLTF_TYPE_VEC4:
                return 4;
            case TINYGLTF_TYPE_MAT2:
                return 4;
            case TINYGLTF_TYPE_MAT3:
                return 9;
            case TINYGLTF_TYPE_MAT4:
                return 16;
            }
        };
        for (auto i = 0u; i < tempData.size(); ++i)
        {
            if (attribIndices[i] > -1)
            {
                const auto& accessor = m_GLTFScene.accessors[attribIndices[i]];

                switch (accessor.componentType)
                {
                case GL_FLOAT:
                {
                    std::size_t componentCount = getComponentCount(accessor.type);
                    tempData[i] = std::make_pair(std::vector<float>(), componentCount);

                    CRO_ASSERT(accessor.bufferView < m_GLTFScene.bufferViews.size(), "");
                    const auto& view = m_GLTFScene.bufferViews[accessor.bufferView];
                    CRO_ASSERT(view.buffer < m_GLTFScene.buffers.size(), "");
                    const auto& buffer = m_GLTFScene.buffers[view.buffer];

                    for (auto j = 0u; j < accessor.count; ++j)
                    {
                        std::size_t index = view.byteOffset + accessor.byteOffset;
                        if (view.byteStride)
                        {
                            index += view.byteStride * j;
                        }
                        else
                        {
                            index += componentCount * sizeof(float) * j;
                        }
                        
                        for (auto k = 0u; k < componentCount; ++k)
                        {
                            float f = 0.f;
                            std::memcpy(&f, &buffer.data[index], sizeof(float));
                            tempData[i].first.push_back(f);
                            index += sizeof(float);
                        }
                    }
                }
                    break;
                case GL_UNSIGNED_BYTE:
                {
                    //TODO in the case of blend indices this is actually
                    //wrong to convert to float, as well as it bloating
                    //the vertex data with unnecessary bytes...
                    //but some bright spark decided long ago that the
                    //vertex data should always be interleaved...
                    std::size_t componentCount = getComponentCount(accessor.type);
                    tempData[i] = std::make_pair(std::vector<float>(), componentCount);

                    CRO_ASSERT(accessor.bufferView < m_GLTFScene.bufferViews.size(), "");
                    const auto& view = m_GLTFScene.bufferViews[accessor.bufferView];
                    CRO_ASSERT(view.buffer < m_GLTFScene.buffers.size(), "");
                    const auto& buffer = m_GLTFScene.buffers[view.buffer];

                    for (auto j = 0u; j < accessor.count; ++j)
                    {
                        std::size_t index = view.byteOffset + accessor.byteOffset + (std::max(componentCount, view.byteStride) * j);

                        for (auto k = 0u; k < componentCount; ++k)
                        {
                            std::uint8_t v = 0;
                            std::memcpy(&v, &buffer.data[index + k], sizeof(std::uint8_t));
                            if (i == cro::Mesh::Attribute::BlendIndices)
                            {
                                tempData[i].first.push_back(static_cast<float>(v));
                            }
                            else
                            {
                                //normalise (probably blendweight, could be UV though...)
                                float f = static_cast<float>(v) / std::numeric_limits<std::uint8_t>::max();
                                tempData[i].first.push_back(f);
                            }
                        }
                    }
                }
                    break;
                case GL_UNSIGNED_SHORT:
                {
                    std::size_t componentCount = getComponentCount(accessor.type);
                    tempData[i] = std::make_pair(std::vector<float>(), componentCount);

                    CRO_ASSERT(accessor.bufferView < m_GLTFScene.bufferViews.size(), "");
                    const auto& view = m_GLTFScene.bufferViews[accessor.bufferView];
                    CRO_ASSERT(view.buffer < m_GLTFScene.buffers.size(), "");
                    const auto& buffer = m_GLTFScene.buffers[view.buffer];

                    for (auto j = 0u; j < accessor.count; ++j)
                    {
                        std::size_t index = view.byteOffset + accessor.byteOffset;
                        if (view.byteStride)
                        {
                            index += view.byteStride * j;
                        }
                        else
                        {
                            index += componentCount * sizeof(std::uint16_t) * j;
                        }
                        
                        for (auto k = 0u; k < componentCount; ++k)
                        {
                            std::uint16_t v = 0;
                            std::memcpy(&v, &buffer.data[index], sizeof(std::uint16_t));
                            if (i == cro::Mesh::Attribute::BlendIndices)
                            {
                                tempData[i].first.push_back(static_cast<float>(v));
                            }
                            else
                            {
                                //normalise (probably blendweight, could be UV though...)
                                float f = static_cast<float>(v) / std::numeric_limits<std::uint16_t>::max();
                                tempData[i].first.push_back(f);
                            }
                            index += sizeof(std::uint16_t);
                        }
                    }
                }
                    break;
                case GL_UNSIGNED_INT:
                {
                    //TODO UVs may not be normalised and will
                    //have to be converted here. In the case of UVs
                    //in pixels we'll need to have loaded the texture
                    //data so we have an image size to normalise with
                }
                    break;
                default: break;
                }
            }
        }

        CRO_ASSERT(!tempData[0].first.empty(), "Position data missing");

        auto vertexCount = tempData[0].first.size() / tempData[0].second;
        for (auto i = 0u; i < vertexCount; ++i)
        {
            for (auto j = 0u; j < tempData.size(); ++j)
            {
                const auto& [data, size] = tempData[j];

                if (data.empty())
                {
                    continue;
                }

                if (j == cro::Mesh::Attribute::Normal)
                {
                    auto index = i * size;
                    glm::vec3 normal(data[index], data[index + 1], data[index + 2]);
                    normal = glm::normalize(normal);

                    vertices.push_back(normal.x);
                    vertices.push_back(normal.y);
                    vertices.push_back(normal.z);
                }
                else if (j == cro::Mesh::Attribute::Tangent)
                {
                    //if (!data.empty())
                    {
                        //calc bitan and correct for sign
                        //NOTE we flip the sign because we also flip the UV
                        auto index = i * size;
                        auto normalIndex = i * (size - 1); //tangents have extra component containing sign
                        const auto& normalData = tempData[cro::Mesh::Attribute::Normal].first;

                        glm::vec3 normal(normalData[normalIndex], normalData[normalIndex + 1], normalData[normalIndex + 2]);
                        glm::vec3 tan(data[index], data[index + 1], data[index + 2]);
                        //tan = normalMat * tan;
                        float sign = std::min(1.f, std::max(-1.f, data[index + 3]));

                        normal = glm::normalize(normal);
                        tan = glm::normalize(tan);

                        glm::vec3 bitan = glm::normalize(glm::cross(normal, tan) * -sign);

                        vertices.push_back(tan.x);
                        vertices.push_back(tan.y);
                        vertices.push_back(tan.z);

                        vertices.push_back(bitan.x);
                        vertices.push_back(bitan.y);
                        vertices.push_back(bitan.z);
                    }
                }
                else if (j == cro::Mesh::Attribute::UV0
                    || (j == cro::Mesh::Attribute::UV1 && !skipSecondUV))
                {
                    //flip the V
                    auto index = i * size;
                    glm::vec2 uv(data[index], data[index + 1]);
                    uv.t = 1.f - uv.t;
                    vertices.push_back(uv.s);
                    vertices.push_back(uv.t);
                }
                else if (j == cro::Mesh::Attribute::Colour)
                {
                    //blender converts colour to sRGB space
                    //and we mostly want linear (at least, I do :) )
                    auto index = i * size;
                    if (convertVertexColourspace)
                    {
                        vertices.push_back(srgb::linearToSrgb(data[index]));
                        vertices.push_back(srgb::linearToSrgb(data[index + 1]));
                        vertices.push_back(srgb::linearToSrgb(data[index + 2]));
                        vertices.push_back(srgb::linearToSrgb(data[index + 3]));
                    }
                    else
                    {
                        vertices.push_back(data[index]);
                        vertices.push_back(data[index+1]);
                        vertices.push_back(data[index+2]);
                        vertices.push_back(data[index+3]);
                    }
                }
                else
                {
                    //if (!data.empty())
                    {
                        for (auto k = i * size; k < (i * size) + size; ++k)
                        {
                            vertices.push_back(data[k]);
                        }
                    }
                }
            }
        }

        //copy the index data
        indices.emplace_back();
        if (prim.indices > -1)
        {
            CRO_ASSERT(prim.indices < m_GLTFScene.accessors.size(), "");
            const auto& accessor = m_GLTFScene.accessors[prim.indices];
            CRO_ASSERT(accessor.bufferView < m_GLTFScene.bufferViews.size(), "");
            const auto& view = m_GLTFScene.bufferViews[accessor.bufferView];
            CRO_ASSERT(view.buffer < m_GLTFScene.buffers.size(), "");
            const auto& buffer = m_GLTFScene.buffers[view.buffer];

            for (auto j = 0u; j < accessor.count; ++j)
            {
                switch (accessor.componentType)
                {
                default:
                    LogW << "Unknown index type " << accessor.componentType << std::endl;
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                {
                    std::uint8_t v = 0;
                    std::size_t index = view.byteOffset + accessor.byteOffset + j;
                    std::memcpy(&v, &buffer.data[index], sizeof(std::uint8_t));
                    indices.back().push_back(v + static_cast<std::uint8_t>(indexOffset & 0xff));
                }
                break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                {
                    std::uint16_t v = 0;
                    std::size_t index = view.byteOffset + accessor.byteOffset + (j * sizeof(std::uint16_t));
                    std::memcpy(&v, &buffer.data[index], sizeof(std::uint16_t));
                    indices.back().push_back(v + static_cast<std::uint16_t>(indexOffset & 0xffff));
                }
                break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                {
                    std::uint32_t v = 0;
                    std::size_t index = view.byteOffset + accessor.byteOffset + (j * sizeof(std::uint32_t));
                    std::memcpy(&v, &buffer.data[index], sizeof(std::uint32_t));
                    indices.back().push_back(v + static_cast<std::uint32_t>(indexOffset & 0xffffffff));
                }
                break;
                }
            }
        }
        else
        {
            auto vertCount = tempData[0].first.size() / tempData[0].second;
            for (auto j = 0u; j < vertCount; ++j)
            {
                indices.back().push_back(static_cast<std::uint32_t>(j + indexOffset));
            }
        }

        primitiveTypes.push_back(prim.mode); //GL_TRIANGLES etc.

        //offset indices for submesh as we're concatting the vertex data
        indexOffset += vertexCount;

        //prim.material; //TODO parse this later
    }

    CMFHeader header;
    header.flags = attribFlags;
    header.animated = loadAnims;
    updateImportNode(header, vertices, indices);

    return true;
}