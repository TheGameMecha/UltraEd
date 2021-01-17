#ifndef _ACTOR_H_
#define _ACTOR_H_

#include <vector>
#include "VertexBuffer.h"
#include "Savable.h"
#include "Util.h"
#include "Collider.h"

namespace UltraEd
{
    enum class ActorType
    {
        Model, Camera
    };

    class Actor : public Savable
    {
    public:
        Actor();
        virtual ~Actor() { }
        virtual void Release();
        virtual void Render(IDirect3DDevice9 *device, ID3DXMatrixStack *stack);
        const boost::uuids::uuid &GetId() { return m_id; }
        void ResetId() { m_id = Util::NewUuid(); }
        const std::string &GetName() { return m_name; }
        bool SetName(std::string name) { return Dirty([&] {  m_name = name.empty() ? "Actor" : name; }, &m_name); }
        const ActorType &GetType() { return m_type; }
        D3DXMATRIX GetMatrix();
        const D3DXMATRIX GetRotationMatrix(bool worldSpace = true);
        void SetLocalRotationMatrix(const D3DXMATRIX &mat) { m_localRot = mat; }
        bool Move(const D3DXVECTOR3 &position) { return Dirty([&] { m_position += position; }, &m_position); }
        bool Scale(const D3DXVECTOR3 &position) { return Dirty([&] { m_scale += position; }, &m_scale); }
        bool Rotate(const float &angle, const D3DXVECTOR3 &dir);
        const D3DXVECTOR3 GetPosition(bool worldSpace = true);
        bool SetPosition(const D3DXVECTOR3 &position) { return Dirty([&] { m_position = position; }, &m_position); }
        const D3DXVECTOR3 &GetEulerAngles();
        bool SetRotation(const D3DXVECTOR3 &eulerAngles);
        const D3DXVECTOR3 &GetScale() { return m_scale; }
        bool SetScale(const D3DXVECTOR3 &scale) { return Dirty([&] { m_scale = scale; }, &m_scale); }
        D3DXVECTOR3 GetRight();
        D3DXVECTOR3 GetForward();
        D3DXVECTOR3 GetUp();
        void GetAxisAngle(D3DXVECTOR3 *axis, float *angle);
        const std::vector<Vertex> &GetVertices() { return m_vertices; }
        bool Pick(const D3DXVECTOR3 &orig, const D3DXVECTOR3 &dir, float *dist);
        const std::string &GetScript() { return m_script; }
        void SetScript(const std::string &script) { Dirty([&] { m_script = script; }, &m_script); }
        Collider *GetCollider() { return m_collider.get(); }
        void SetCollider(Collider *collider) { Dirty([&] { m_collider = std::shared_ptr<Collider>(collider); }, &m_collider); }
        bool HasCollider() { return GetCollider() != NULL; }
        Actor *GetParent() { return m_parent; }
        void SetParent(Actor *actor);
        void UnParent();
        const std::map<boost::uuids::uuid, Actor *> &GetChildren() { return m_children; }
        nlohmann::json Save();
        void Load(const nlohmann::json &root);

    protected:
        std::shared_ptr<VertexBuffer> m_vertexBuffer;
        ActorType m_type;
        void Import(const char *filePath);

    private:
        boost::uuids::uuid m_id;
        std::string m_name;
        std::vector<Vertex> m_vertices;
        D3DMATERIAL9 m_material;
        D3DXVECTOR3 m_position;
        D3DXVECTOR3 m_scale;
        D3DXMATRIX m_localRot;
        D3DXMATRIX m_worldRot;
        D3DXVECTOR3 m_eulerAngles;
        std::string m_script;
        bool IntersectTriangle(const D3DXVECTOR3 &orig, const D3DXVECTOR3 &dir,
            const D3DXVECTOR3 &v0, const D3DXVECTOR3 &v1, const D3DXVECTOR3 &v2, float *dist);
        std::shared_ptr<Collider> m_collider;
        Actor *m_parent;
        std::map<boost::uuids::uuid, Actor *> m_children;
    };
}

#endif
