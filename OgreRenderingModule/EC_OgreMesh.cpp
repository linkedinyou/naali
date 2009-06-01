// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "Foundation.h"
#include "OgreRenderingModule.h"
#include "Renderer.h"
#include "EC_OgrePlaceable.h"
#include "EC_OgreMesh.h"
#include "RexTypes.h"

#include <Ogre.h>

namespace OgreRenderer
{
    EC_OgreMesh::EC_OgreMesh(Foundation::ModuleInterface* module) :
        Foundation::ComponentInterface(module->GetFramework()),
        renderer_(checked_static_cast<OgreRenderingModule*>(module)->GetRenderer()),
        entity_(NULL),
        adjustment_node_(NULL),
        attached_(false)
    {
        Ogre::SceneManager* scene_mgr = renderer_->GetSceneManager();
        adjustment_node_ = scene_mgr->createSceneNode();
    }
    
    EC_OgreMesh::~EC_OgreMesh()
    {
        RemoveMesh();
        
        if (adjustment_node_)
        {            
            Ogre::SceneManager* scene_mgr = renderer_->GetSceneManager();
            scene_mgr->destroySceneNode(adjustment_node_);
            adjustment_node_ = NULL;
        }        
    }
    
    void EC_OgreMesh::SetPlaceable(Foundation::ComponentPtr placeable)
    {
        if (!dynamic_cast<EC_OgrePlaceable*>(placeable.get()))
        {
            OgreRenderingModule::LogError("Attempted to set placeable which is not " + NameStatic());
            return;
        }
        
        DetachEntity();
        placeable_ = placeable;
        AttachEntity();
    }
    
    void EC_OgreMesh::SetAdjustPosition(const Core::Vector3df& position)
    {
        adjustment_node_->setPosition(Ogre::Vector3(position.x, position.y, position.z));
    }

    void EC_OgreMesh::SetAdjustOrientation(const Core::Quaternion& orientation)
    {
        adjustment_node_->setOrientation(Ogre::Quaternion(orientation.w, orientation.x, orientation.y, orientation.z));
    }   
    
    void EC_OgreMesh::SetAdjustScale(const Core::Vector3df& scale)
    {
        adjustment_node_->setScale(Ogre::Vector3(scale.x, scale.y, scale.z));
    }
    
    Core::Vector3df EC_OgreMesh::GetAdjustPosition() const
    {
        const Ogre::Vector3& pos = adjustment_node_->getPosition();
        return Core::Vector3df(pos.x, pos.y, pos.z);
    }
    
    Core::Quaternion EC_OgreMesh::GetAdjustOrientation() const
    {
        const Ogre::Quaternion& orientation = adjustment_node_->getOrientation();
        return Core::Quaternion(orientation.x, orientation.y, orientation.z, orientation.w);
    }     
    
    Core::Vector3df EC_OgreMesh::GetAdjustScale() const
    {
        const Ogre::Vector3& scale = adjustment_node_->getScale();
        return Core::Vector3df(scale.x, scale.y, scale.z);
    }
    
    bool EC_OgreMesh::SetMesh(const std::string& mesh_name)
    {
        RemoveMesh();
        
        Ogre::SceneManager* scene_mgr = renderer_->GetSceneManager();

        Ogre::MeshPtr mesh = Ogre::MeshManager::getSingleton().getByName(mesh_name);
        // Note: for resources loaded from disk, the resource might not exist until attempted to use in an entity for the first time
        if (!mesh.isNull())
        {
            if (mesh->hasSkeleton())
            {
                Ogre::SkeletonPtr skeleton = Ogre::SkeletonManager::getSingleton().getByName(mesh->getSkeletonName());
                if (skeleton.isNull() || skeleton->getNumBones() == 0)
                {
                    OgreRenderingModule::LogWarning("Mesh " + mesh_name + " has a skeleton with 0 bones. Disabling the skeleton.");
                    mesh->setSkeletonName("");
                }
            }
        }
        
        try
        {
            entity_ = scene_mgr->createEntity(renderer_->GetUniqueObjectName(), mesh_name);
        }
        catch (Ogre::Exception& e)
        {
            OgreRenderingModule::LogError("Could not set mesh " + mesh_name + ": " + std::string(e.what()));
            return false;
        }
        
        AttachEntity();
        return true;
    }
    
    void EC_OgreMesh::RemoveMesh()
    {
        if (!entity_)
            return;

        DetachEntity();

        Ogre::SceneManager* scene_mgr = renderer_->GetSceneManager();
        scene_mgr->destroyEntity(entity_);
        entity_ = NULL;
    }
    
    Core::uint EC_OgreMesh::GetNumMaterials()
    {
        if (!entity_)
            return 0;
            
        return entity_->getNumSubEntities();
    }
    
    const std::string& EC_OgreMesh::GetMaterialName(Core::uint index)
    {
        const static std::string empty;
        
        if (!entity_)
            return empty;
        
        if (index >= entity_->getNumSubEntities())
            return empty;
        
        return entity_->getSubEntity(index)->getMaterialName();
    }
    
    bool EC_OgreMesh::SetMaterial(Core::uint index, const std::string& material_name)
    {
        if (!entity_)
        {
            OgreRenderingModule::LogError("Could not set material " + material_name + ": no mesh");
            return false;
        }
        
        if (index >= entity_->getNumSubEntities())
        {
            OgreRenderingModule::LogError("Could not set material " + material_name + ": illegal submesh index " + Core::ToString<Core::uint>(index));
            return false;
        }
        
        try
        {
            entity_->getSubEntity(index)->setMaterialName(material_name);
        }
        catch (Ogre::Exception& e)
        {
            OgreRenderingModule::LogError("Could not set material " + material_name + ": " + std::string(e.what()));
            return false;
        }
        
        return true;
    }
    
    void EC_OgreMesh::DetachEntity()
    {
        if ((!attached_) || (!entity_) || (!placeable_))
            return;
            
        EC_OgrePlaceable* placeable = checked_static_cast<EC_OgrePlaceable*>(placeable_.get());
        Ogre::SceneNode* node = placeable->GetSceneNode();
        adjustment_node_->detachObject(entity_);
        node->removeChild(adjustment_node_);
                
        attached_ = false;
    }
    
    void EC_OgreMesh::AttachEntity()
    {
        if ((attached_) || (!entity_) || (!placeable_))
            return;
            
        EC_OgrePlaceable* placeable = checked_static_cast<EC_OgrePlaceable*>(placeable_.get());
        Ogre::SceneNode* node = placeable->GetSceneNode();
        node->addChild(adjustment_node_);
        adjustment_node_->attachObject(entity_);
                
        attached_ = true;
    }
    
    const std::string& EC_OgreMesh::GetMeshName() const
    {
        static std::string empty_name;
        
        if (!entity_)
            return empty_name;
        else
            return entity_->getMesh()->getName();
    }
    
    void EC_OgreMesh::SetCastShadows(bool enabled)
    {
        if (entity_)
            entity_->setCastShadows(enabled);
    }        
    
    void EC_OgreMesh::GetBoundingBox(Core::Vector3df& min, Core::Vector3df& max)
    {
        if (!entity_)
        {
            min = Core::Vector3df(0.0, 0.0, 0.0);
            max = Core::Vector3df(0.0, 0.0, 0.0);
            return;
        }
     
        const Ogre::AxisAlignedBox& bbox = entity_->getMesh()->getBounds();
        const Ogre::Vector3& bboxmin = bbox.getMinimum();
        const Ogre::Vector3& bboxmax = bbox.getMaximum();
        
        min = Core::Vector3df(bboxmin.x, bboxmin.y, bboxmin.z);
        max = Core::Vector3df(bboxmax.x, bboxmax.y, bboxmax.z);
    }
}