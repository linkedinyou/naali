/**
 *  For conditions of distribution and use, see copyright notice in license.txt
 *
 *  @file   EC_Highlight.h
 *  @brief  EC_Highlight enables visual highlighting effect for of scene entity.
 *  @note   The entity must have EC_Mesh component available to be useful
 */

#ifndef incl_EC_Highlight_EC_Highlight_h
#define incl_EC_Highlight_EC_Highlight_h

#include "IComponent.h"
#include "AssetFwd.h"
#include "Declare_EC.h"
#include "Color.h"

namespace OgreRenderer
{
    class Renderer;
}

namespace Ogre
{
    class SceneNode;
    class Entity;
}

class OgreMaterialAsset;
class EC_Mesh;

/**
<table class="header">
<tr>
<td>
<h2>HighLight</h2>
Enables visual highlighting effect of scene entity.

<b>Attributes</b>:
<ul>
<li> bool: visible
<div> Whether the highlight effect is shown.  </div>
<li> Color: solidColor
<div> Color of the solid fill pass. </div>
<li> Color: outlineColor
<div> Color of the outline (wireframe) pass. </div>

<b>Exposes the following scriptable functions:</b>
<ul>
<li>"Hide": Disables the highlighting effect.
<li>"Show": Shows the highlighting effect.
<li>"IsVisible": Returns if the highlight component is visible or not.
  @true If the highlight component is visible, false if it's hidden or not initialized properly.
</ul>

<b>Reacts on the following actions:</b>
<ul>
<li>...
</ul>
</td>
</tr>

Does not emit any actions.

<b>Depends on components Placeable and OgreMesh</b>. 
</table>
*/

class EC_Highlight : public IComponent
{
    Q_OBJECT
    DECLARE_EC(EC_Highlight);

public:
    /// Destructor.
    ~EC_Highlight();

    /// Visible flag. If true,the mesh component in the same entity will be highlighted
    Q_PROPERTY(bool visible READ getvisible WRITE setvisible)
    DEFINE_QPROPERTY_ATTRIBUTE(bool, visible)
    
    /// Color of the solid fill pass
    Q_PROPERTY(Color solidColor READ getsolidColor WRITE setsolidColor);
    DEFINE_QPROPERTY_ATTRIBUTE(Color, solidColor);
    
    /// Color of the outline (wireframe) pass
    Q_PROPERTY(Color outlineColor READ getoutlineColor WRITE setoutlineColor);
    DEFINE_QPROPERTY_ATTRIBUTE(Color, outlineColor);
    
public slots:
    /// Shows the highlighting effect. Does not change the visible attribute
    void Show();

    /// Hides the highlighting effect. Does not change the visible attribute
    void Hide();

    /// Returns if the highlight component is visible or not.
    /// @true If the highlight component is visible, false if it's hidden or not initialized properly.
    bool IsVisible() const;

private slots:
    /// Called when the parent entity has been set.
    void UpdateSignals();

    /// Called when some of the attributes has been changed.
    void OnAttributeUpdated(IAttribute *attribute);

    /// Called when component has been removed from the parent entity.
    void OnComponentRemoved(IComponent* component, AttributeChange::Type change);
    
    /// Try to acquire the mesh component from this entity
    void AcquireMesh();
    
    /// Trigger delayed reapply of the highlight
    void TriggerReapply();
    
    /// Actually do a reclone/reapply of highlight materials when mesh or materials changed
    void ReapplyHighlight();
    
private:
    /// Create highlight pass to an Ogre material's all techniques
    void CreateHighlightToOgreMaterial(OgreMaterialAsset* mat);
    
    /// Apply a color change to all existing highlight materials
    void ApplyHighlightColors();
    
    /// Constuctor.
    /// @param module Owner module.
    explicit EC_Highlight(IModule *module);
    
    /// Mesh component pointer
    boost::weak_ptr<EC_Mesh> mesh_;
    
    /// Highlight material assets, cloned from the mesh component's materials
    std::vector<AssetPtr> materials_;
    
    /// Delayed reapply already pending -flag
    bool reapplyPending_;
    /// In reapply -flag. Used to disregard material change signals that come as a result of our own actions
    bool inApply_;
};

#endif
