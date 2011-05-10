/**
 *  For conditions of distribution and use, see copyright notice in license.txt
 *
 *  @file   EC_Highlight.cpp
 *  @brief  EC_Highlight enables visual highlighting effect for of scene entity.
 *  @note   The entity must have EC_Mesh component available to be useful
 */

#include "DebugOperatorNew.h"
#include "EC_Highlight.h"
#include "IModule.h"
#include "Entity.h"
#include "Renderer.h"
#include "OgreMaterialUtils.h"
#include "OgreMaterialAsset.h"
#include "EC_Placeable.h"
#include "EC_Mesh.h"
#include "LoggingFunctions.h"
#include "AssetAPI.h"

#include <QTimer>

#include "MemoryLeakCheck.h"

#include <OgreMaterial.h>
#include <OgreTechnique.h>
#include <OgrePass.h>

EC_Highlight::EC_Highlight(IModule *module) :
    IComponent(module->GetFramework()),
    visible(this, "Is visible", false),
    solidColor(this, "Solid color", Color(0.3f, 0.5f, 0.1f, 0.5f)),
    outlineColor(this, "Outline color", Color(1.0f, 1.0f, 1.0f, 0.5f)),
    reapplyPending_(false),
    inApply_(false)
{
    connect(this, SIGNAL(ParentEntitySet()), SLOT(UpdateSignals()));
    connect(this, SIGNAL(AttributeChanged(IAttribute*, AttributeChange::Type)), SLOT(OnAttributeUpdated(IAttribute*)));
}

EC_Highlight::~EC_Highlight()
{
    Hide();
}

void  EC_Highlight::Show()
{
    if (mesh_.expired())
        return;
    
    inApply_ = true;
    
    // Remove old materials first if they exist
    Hide();
    
    EC_Mesh* mesh = mesh_.lock().get();
    AssetAPI* assetAPI = framework_->Asset();
    entity_id_t entityID = GetParentEntity()->GetId();
    
    // Clone all valid material assets that we can find from the mesh
    /// \todo Currently will clone the same material several times if used on several submeshes
    /// \todo What if the material is yet pending, or is not an asset (LitTextured)
    AssetReferenceList materialList = mesh->meshMaterial.Get();
    for(int i = 0; i < materialList.Size(); ++i)
    {
        if (!materialList[i].ref.isEmpty())
        {
            QString assetFullName = assetAPI->ResolveAssetRef("", materialList[i].ref);
            AssetPtr asset = assetAPI->GetAsset(assetFullName);
            if ((asset) && (asset->IsLoaded()) && (dynamic_cast<OgreMaterialAsset*>(asset.get())))
            {
                /// \todo The material emits an error print when cloning, due to attempting to resolve already sanitated assetrefs
                AssetPtr clone = asset->Clone("EC_Highlight_" + QString::number(entityID) + "_" + QString::number(i) + ".material");
                if (clone)
                {
                    OgreMaterialAsset* matAsset = dynamic_cast<OgreMaterialAsset*>(clone.get());
                    CreateHighlightToOgreMaterial(matAsset);
                    mesh->SetMaterial(i, clone->Name());
                    materials_.push_back(clone);
                }
            }
        }
    }
    
    inApply_ = false;
}

void EC_Highlight::Hide()
{
    if (!mesh_.expired())
    {
        // Restore mesh component's original materials to hide highlight effect
        EC_Mesh* mesh = mesh_.lock().get();
        mesh->ApplyMaterial();
    }
    
    // Destroy all the highlight materials
    AssetAPI* assetAPI = framework_->Asset();
    for (unsigned i = 0; i < materials_.size(); ++i)
        assetAPI->ForgetAsset(materials_[i], false);
    materials_.clear();
}

bool EC_Highlight::IsVisible() const
{
    return materials_.size() > 0;
}

void EC_Highlight::UpdateSignals()
{
    Entity* parent = GetParentEntity();
    if (parent)
    {
        // Connect to ComponentAdded/Removed signals of the parent entity, so we can check when the mesh component gets added or removed
        connect(parent, SIGNAL(ComponentAdded(IComponent*, AttributeChange::Type)), SLOT(AcquireMesh()));
        connect(parent, SIGNAL(ComponentRemoved(IComponent*, AttributeChange::Type)), SLOT(OnComponentRemoved(IComponent*, AttributeChange::Type)));
        // Also try to acquire the mesh component immediately
        AcquireMesh();
    }
}

void EC_Highlight::OnComponentRemoved(IComponent* component, AttributeChange::Type change)
{
    if (component == mesh_.lock().get())
    {
        Hide();
        mesh_.reset();
    }
}

void EC_Highlight::AcquireMesh()
{
    // Return if already acquired
    if (!mesh_.expired())
        return;
    
    mesh_ = GetParentEntity()->GetComponent<EC_Mesh>();
    
    EC_Mesh* mesh = mesh_.lock().get();
    if (mesh)
    {
        // Connect to mesh & material change signals so we can reapply the highlight
        connect(mesh, SIGNAL(MeshChanged()), this, SLOT(TriggerReapply()), Qt::UniqueConnection);
        connect(mesh, SIGNAL(MaterialChanged(uint, const QString &)), this, SLOT(TriggerReapply()), Qt::UniqueConnection);
        
        // Also show right now if highlight active
        if (visible.Get())
            Show();
    }
}

void EC_Highlight::OnAttributeUpdated(IAttribute *attribute)
{
    if (attribute == &visible)
    {
        if (visible.Get())
            Show();
        else
            Hide();
    }
    
    if ((attribute == &solidColor) || (attribute == &outlineColor))
        ApplyHighlightColors();
}

void EC_Highlight::TriggerReapply()
{
    // Disregard signal if in the middle of applying highlight
    if (inApply_)
        return;
    
    // We might get multiple requests to reapply, but actually execute on the next frame, so that we don't do needless recreation of materials
    if ((!reapplyPending_) && (visible.Get()))
    {
        reapplyPending_ = true;
        QTimer::singleShot(0, this, SLOT(ReapplyHighlight()));
    }
}

void EC_Highlight::ReapplyHighlight()
{
    if (visible.Get())
        Show();
    reapplyPending_ = false;
}

void EC_Highlight::CreateHighlightToOgreMaterial(OgreMaterialAsset* mat)
{
    if (!mat)
        return;
    
    unsigned numTech = mat->GetNumTechniques();
    for (unsigned i = 0; i < numTech; ++i)
    {
        int pass1 = mat->CreatePass(i);
        int pass2 = mat->CreatePass(i);
        
        // Setting the shaders requires the SolidAmbient.material to exist in the resource groups, so that the shaders exist
        mat->SetLighting(i, pass1, false);
        mat->SetAlphaBlend(i, pass1);
        mat->SetDepthWrite(i, pass1, false);
        mat->SetDepthBias(i, pass1, 0.001f);
        mat->SetShaders(i, pass1, "SolidAmbientVP", "SolidAmbientFP");
        mat->SetAmbientColor(i, pass1, solidColor.Get());
        
        mat->SetLighting(i, pass2, false);
        mat->SetAlphaBlend(i, pass2);
        mat->SetDepthWrite(i, pass2, false);
        mat->SetDepthBias(i, pass2, 0.002f);
        mat->SetShaders(i, pass2, "SolidAmbientVP", "SolidAmbientFP");
        mat->SetAmbientColor(i, pass2, outlineColor.Get());
        mat->SetWireframe(i, pass2);
    }
}

void EC_Highlight::ApplyHighlightColors()
{
    for (unsigned i = 0; i < materials_.size(); ++i)
    {
        OgreMaterialAsset* mat = dynamic_cast<OgreMaterialAsset*>(materials_[i].get());
        unsigned numTech = mat->GetNumTechniques();
        for (unsigned i = 0; i < numTech; ++i)
        {
            unsigned numPasses = mat->GetNumPasses(i);
            // Modify the diffuse color of the last 2 passes
            // (this should be a highlight material that we added the 2 passes to)
            if (numPasses >= 2)
            {
                mat->SetAmbientColor(i, numPasses - 2, solidColor.Get());
                mat->SetAmbientColor(i, numPasses - 1, outlineColor.Get());
            }
        }
    }
}
