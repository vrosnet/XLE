// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "RetainedEntities.h"
#include "../../Utility/StringUtils.h"
#include "../../Utility/PtrUtils.h"
#include "../../Utility/Streams/StreamFormatter.h"

namespace EntityInterface
{
    bool RetainedEntities::SetSingleProperties(
        RetainedEntity& dest, const RegisteredObjectType& type, const PropertyInitializer& prop) const
    {
        if (prop._prop == 0 || prop._prop > type._properties.size()) return false;
        if (!prop._src) return false;

        auto typeHint = prop._isString ? ImpliedTyping::TypeHint::String : ImpliedTyping::TypeHint::None;

        const auto& propertyName = type._properties[prop._prop-1];
        dest._properties.SetParameter(
            propertyName.c_str(), prop._src, 
            ImpliedTyping::TypeDesc((ImpliedTyping::TypeCat)prop._elementType, (uint16)prop._arrayCount, typeHint));
        return true;
    }

    auto RetainedEntities::GetObjectType(ObjectTypeId id) const -> RegisteredObjectType*
    {
        for (auto i=_registeredObjectTypes.begin(); i!=_registeredObjectTypes.end(); ++i)
            if (i->first == id) return &i->second;
        return nullptr;
    }

    bool RetainedEntities::RegisterCallback(ObjectTypeId typeId, OnChangeDelegate onChange)
    {
        auto type = GetObjectType(typeId);
        if (!type) return false;
        type->_onChange.push_back(std::move(onChange));
        return true;
    }

    void RetainedEntities::InvokeOnChange(RegisteredObjectType& type, RetainedEntity& obj, ChangeType changeType) const
    {
        for (auto i=type._onChange.begin(); i!=type._onChange.end(); ++i) {
            (*i)(*this, Identifier(obj._doc, obj._id, obj._type), changeType);
        }

        if ((   changeType == ChangeType::SetProperty || changeType == ChangeType::ChildSetProperty 
            ||  changeType == ChangeType::AddChild || changeType == ChangeType::RemoveChild
            ||  changeType == ChangeType::ChangeHierachy || changeType == ChangeType::Delete)
            &&  obj._parent != 0) {

            ChangeType newChangeType = ChangeType::ChildSetProperty;
            if (    changeType == ChangeType::AddChild || changeType == ChangeType::RemoveChild
                ||  changeType == ChangeType::ChangeHierachy || changeType == ChangeType::Delete)
                newChangeType = ChangeType::ChangeHierachy;

            for (auto i=_objects.begin(); i!=_objects.end(); ++i)
                if (i->_id == obj._parent && i->_doc == obj._doc) {
                    auto type = GetObjectType(i->_type);
                    if (type) 
                        InvokeOnChange(*type, *i, newChangeType);
                }
        }
    }

    auto RetainedEntities::GetEntity(DocumentId doc, ObjectId obj) const -> const RetainedEntity*
    {
        for (auto i=_objects.begin(); i!=_objects.end(); ++i)
            if (i->_id == obj && i->_doc == doc) {
                return AsPointer(i);
            }
        return nullptr;
    }

    auto RetainedEntities::GetEntity(const Identifier& id) const -> const RetainedEntity*
    {
        for (auto i=_objects.begin(); i!=_objects.end(); ++i)
            if (i->_id == id.Object() && i->_doc == id.Document() && i->_type == id.ObjectType())
                return AsPointer(i);
        return nullptr;
    }

    auto RetainedEntities::GetEntityInt(DocumentId doc, ObjectId obj) const -> RetainedEntity* 
    {
        for (auto i=_objects.begin(); i!=_objects.end(); ++i)
            if (i->_id == obj && i->_doc == doc) {
                return AsPointer(i);
            }
        return nullptr;
    }

    auto RetainedEntities::FindEntitiesOfType(ObjectTypeId typeId) const -> std::vector<const RetainedEntity*>
    {
        std::vector<const RetainedEntity*> result;
        for (auto i=_objects.begin(); i!=_objects.end(); ++i)
            if (i->_type == typeId) {
                result.push_back(AsPointer(i));
            }
        return std::move(result);
    }

    ObjectTypeId RetainedEntities::GetTypeId(const utf8 name[]) const
    {
        for (auto i=_registeredObjectTypes.cbegin(); i!=_registeredObjectTypes.cend(); ++i)
            if (!XlCompareStringI(i->second._name.c_str(), name))
                return i->first;
        
        _registeredObjectTypes.push_back(
            std::make_pair(_nextObjectTypeId, RegisteredObjectType(name)));
        return _nextObjectTypeId++;
    }

	PropertyId RetainedEntities::GetPropertyId(ObjectTypeId typeId, const utf8 name[]) const
    {
        auto type = GetObjectType(typeId);
        if (!type) return 0;

        for (auto i=type->_properties.cbegin(); i!=type->_properties.cend(); ++i)
            if (!XlCompareStringI(i->c_str(), name)) 
                return (PropertyId)(1+std::distance(type->_properties.cbegin(), i));
        
        type->_properties.push_back(name);
        return (PropertyId)type->_properties.size();
    }

	ChildListId RetainedEntities::GetChildListId(ObjectTypeId typeId, const utf8 name[]) const
    {
        auto type = GetObjectType(typeId);
        if (!type) return 0;

        for (auto i=type->_childLists.cbegin(); i!=type->_childLists.cend(); ++i)
            if (!XlCompareStringI(i->c_str(), name)) 
                return (PropertyId)std::distance(type->_childLists.cbegin(), i);
        
        type->_childLists.push_back(name);
        return (PropertyId)(type->_childLists.size()-1);
    }

    std::basic_string<utf8> RetainedEntities::GetTypeName(ObjectTypeId id) const
    {
        auto i = LowerBound(_registeredObjectTypes, id);
        if (i != _registeredObjectTypes.end() && i->first == id) {
            return i->second._name;
        }
        return std::basic_string<utf8>();
    }

    RetainedEntities::RetainedEntities()
    {
        _nextObjectTypeId = 1;
        _nextObjectId = 1;
    }

    RetainedEntities::~RetainedEntities() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    RetainedEntity::RetainedEntity() {}

    RetainedEntity::RetainedEntity(RetainedEntity&& moveFrom) never_throws
    : _properties(std::move(moveFrom._properties))
    , _children(std::move(moveFrom._children))
    {
        _id = moveFrom._id;
        _doc = moveFrom._doc;
        _type = moveFrom._type;
        _parent = moveFrom._parent;
    }

    RetainedEntity& RetainedEntity::operator=(RetainedEntity&& moveFrom) never_throws
    {
        _properties = std::move(moveFrom._properties);
        _children = std::move(moveFrom._children);
        _id = moveFrom._id;
        _doc = moveFrom._doc;
        _type = moveFrom._type;
        _parent = moveFrom._parent;
        return *this;
    }

    RetainedEntity::~RetainedEntity() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

	ObjectId RetainedEntityInterface::AssignObjectId(DocumentId doc, ObjectTypeId typeId) const
    {
        return _scene->_nextObjectId++;
    }

	bool RetainedEntityInterface::CreateObject(const Identifier& id, 
        const PropertyInitializer initializers[], size_t initializerCount)
    {
        auto type = _scene->GetObjectType(id.ObjectType());
        if (!type) return false;

        for (auto i=_scene->_objects.cbegin(); i!=_scene->_objects.cend(); ++i)
            if (i->_doc == id.Document() && i->_id == id.Object()) return false;

        RetainedEntity newObject;
        newObject._doc = id.Document();
        newObject._id = id.Object();
        newObject._type = id.ObjectType();
        newObject._parent = 0;

        for (size_t c=0; c<initializerCount; ++c)
            _scene->SetSingleProperties(newObject, *type, initializers[c]);

        _scene->_objects.push_back(std::move(newObject));

        _scene->InvokeOnChange(*type, _scene->_objects[_scene->_objects.size()-1], RetainedEntities::ChangeType::Create);
        return true;
    }

	bool RetainedEntityInterface::DeleteObject(const Identifier& id)
    {
        for (auto i=_scene->_objects.begin(); i!=_scene->_objects.end(); ++i)
            if (i->_doc == id.Document() && i->_id == id.Object()) {
                assert(i->_type == id.ObjectType());
                RetainedEntity copy(std::move(*i));
                _scene->_objects.erase(i);

                auto type = _scene->GetObjectType(id.ObjectType());
                if (type)
                    _scene->InvokeOnChange(*type, copy, RetainedEntities::ChangeType::Delete);
                return true;
            }
        return false;
    }

	bool RetainedEntityInterface::SetProperty(
        const Identifier& id, 
        const PropertyInitializer initializers[], size_t initializerCount)
    {
        auto type = _scene->GetObjectType(id.ObjectType());
        if (!type) return false;

        for (auto i=_scene->_objects.begin(); i!=_scene->_objects.end(); ++i)
            if (i->_doc == id.Document() && i->_id == id.Object()) {
                bool gotChange = false;
                for (size_t c=0; c<initializerCount; ++c) {
                    auto& prop = initializers[c];
                    gotChange |= _scene->SetSingleProperties(*i, *type, prop);
                }
                if (gotChange) _scene->InvokeOnChange(*type, *i, RetainedEntities::ChangeType::SetProperty);
                return true;
            }

        return false;
    }

	bool RetainedEntityInterface::GetProperty(const Identifier& id, PropertyId prop, void* dest, unsigned* destSize) const
    {
        auto type = _scene->GetObjectType(id.ObjectType());
        if (!type) return false;
        if (prop == 0 || prop > type->_properties.size()) return false;

        const auto& propertyName = type->_properties[prop-1];

        for (auto i=_scene->_objects.begin(); i!=_scene->_objects.end(); ++i)
            if (i->_doc == id.Document() && i->_id == id.Object()) {
                auto res = i->_properties.GetParameter<unsigned>(propertyName.c_str());
                if (res.first) {
                    *(unsigned*)dest = res.second;
                }
                return true;
            }

        return false;
    }

    bool RetainedEntityInterface::SetParent(
        const Identifier& child, const Identifier& parent, int insertionPosition)
    {
        if (child.Document() != parent.Document())
            return false;

        auto childType = _scene->GetObjectType(child.ObjectType());
        if (!childType) return false;

        auto* childObj = _scene->GetEntityInt(child.Document(), child.Object());
        if (!childObj || childObj->_type != child.ObjectType())
            return false;

        if (childObj->_parent != 0) {
            auto* oldParent = _scene->GetEntityInt(child.Document(), childObj->_parent);
            if (oldParent) {
                auto i = std::find(oldParent->_children.begin(), oldParent->_children.end(), child.Object());
                oldParent->_children.erase(i);

                auto oldParentType = _scene->GetObjectType(parent.ObjectType());
                if (oldParentType)
                    _scene->InvokeOnChange(
                        *oldParentType, *oldParent, 
                        RetainedEntities::ChangeType::RemoveChild);
            }

            childObj->_parent = 0;
        }

///////////////////////////////////////////////////////////////////////////////////////////////////
            // if parent is set to 0, then this is a "remove from parent" operation
        if (!parent.Object()) {
            _scene->InvokeOnChange(*childType, *childObj, RetainedEntities::ChangeType::SetParent);
            return true;
        }

        auto* parentObj = _scene->GetEntityInt(parent.Document(), parent.Object());
        if (!parentObj || parentObj->_type != parent.ObjectType()) {
            _scene->InvokeOnChange(*childType, *childObj, RetainedEntities::ChangeType::SetParent);
            return false;
        }

        if (insertionPosition < 0 || insertionPosition >= (int)parentObj->_children.size()) {
            parentObj->_children.push_back(child.Object());
        } else {
            parentObj->_children.insert(
                parentObj->_children.begin() + insertionPosition,
                child.Object());
        }
        childObj->_parent = parentObj->_id;

        _scene->InvokeOnChange(*childType, *childObj, RetainedEntities::ChangeType::SetParent);

        auto parentType = _scene->GetObjectType(parent.ObjectType());
        if (parentType)
            _scene->InvokeOnChange(*parentType, *parentObj, RetainedEntities::ChangeType::AddChild);

        return true;
    }

	ObjectTypeId    RetainedEntityInterface::GetTypeId(const char name[]) const
    {
        return _scene->GetTypeId((const utf8*)name);
    }

	PropertyId      RetainedEntityInterface::GetPropertyId(ObjectTypeId typeId, const char name[]) const
    {
        return _scene->GetPropertyId(typeId, (const utf8*)name);
    }

	ChildListId     RetainedEntityInterface::GetChildListId(ObjectTypeId typeId, const char name[]) const
    {
        return _scene->GetPropertyId(typeId, (const utf8*)name);
    }

    DocumentId RetainedEntityInterface::CreateDocument(DocumentTypeId docType, const char initializer[])
    {
        return 0;
    }

	bool RetainedEntityInterface::DeleteDocument(DocumentId doc, DocumentTypeId docType)
    {
        return false;
    }

    DocumentTypeId RetainedEntityInterface::GetDocumentTypeId(const char name[]) const
    {
        return 0;
    }

	RetainedEntityInterface::RetainedEntityInterface(std::shared_ptr<RetainedEntities> flexObjects)
    : _scene(std::move(flexObjects))
    {}

	RetainedEntityInterface::~RetainedEntityInterface()
    {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    static Identifier DeserializeEntity(
        InputStreamFormatter<utf8>& formatter,
        IEntityInterface& interf,
        DocumentId docId)
    {
        using Blob = InputStreamFormatter<utf8>::Blob;
        using Section = InputStreamFormatter<utf8>::InteriorSection;
        
        utf8 tempBuffer[256];
        
        auto beginLoc = formatter.GetLocation();
        Section objType = { nullptr, nullptr };
        if (!formatter.TryBeginElement(objType))
            Throw(FormatException("Error in begin element in entity file", formatter.GetLocation()));

        XlCopyNString(tempBuffer, objType._start, objType._end - objType._start);
        auto typeId = interf.GetTypeId((const char*)tempBuffer);

        std::vector<PropertyInitializer> inits;
        std::vector<char> initsBuffer;
        initsBuffer.reserve(256);

        std::vector<Identifier> children;

        for (;;) {
            switch (formatter.PeekNext()) {
            case Blob::BeginElement:
                {
                    auto child = DeserializeEntity(formatter, interf, docId);
                    if (child.Object())
                        children.push_back(child);
                }
                break;

            case Blob::AttributeName:
                {
                    Section name, value;
                    if (!formatter.TryAttribute(name, value))
                        Throw(FormatException("Error in begin element in entity file", formatter.GetLocation()));

                        // parse the value and add it as a property initializer
                    char intermediateBuffer[64];
                    auto type = ImpliedTyping::Parse(
                        (const char*)value._start, (const char*)value._end,
                        intermediateBuffer, dimof(intermediateBuffer));

                    size_t bufferOffset = initsBuffer.size();
                    
                    if (type._type == ImpliedTyping::TypeCat::Void) {
                        type._type = ImpliedTyping::TypeCat::UInt8;
                        type._arrayCount = uint16(value._end - value._start);
                        type._typeHint = ImpliedTyping::TypeHint::String;
                        initsBuffer.insert(initsBuffer.end(), value._start, value._end);
                    } else {
                        auto size = std::min(type.GetSize(), (unsigned)sizeof(intermediateBuffer));
                        initsBuffer.insert(initsBuffer.end(), intermediateBuffer, PtrAdd(intermediateBuffer, size));
                    }
               
                    XlCopyNString(tempBuffer, name._start, name._end - name._start);
                    auto id = interf.GetPropertyId(typeId, (const char*)tempBuffer);

                    PropertyInitializer i;
                    i._prop = id;
                    i._elementType = unsigned(type._type);
                    i._arrayCount = type._arrayCount;
                    i._src = (const void*)bufferOffset;
                    i._isString = type._typeHint == ImpliedTyping::TypeHint::String;

                    inits.push_back(i);
                }
                break;

            case Blob::EndElement:
            default:
                if (!formatter.TryEndElement())
                    Throw(FormatException("Expecting end element in entity deserialisation", formatter.GetLocation()));

                if (typeId != ~ObjectTypeId(0x0)) {
                    for (auto&i:inits) i._src = PtrAdd(AsPointer(initsBuffer.cbegin()), size_t(i._src));

                    auto id = interf.AssignObjectId(docId, typeId);
                    Identifier identifier(docId, id, typeId);
                    if (!interf.CreateObject(identifier, AsPointer(inits.cbegin()), inits.size()))
                        Throw(FormatException("Error while creating object in entity deserialisation", beginLoc));

                    for (const auto&c:children)
                        interf.SetParent(c, identifier, -1);

                    typeId = ~ObjectTypeId(0x0);
                    initsBuffer.clear();

                    return identifier;
                }

                return Identifier();
            }
        }
    }

    void Deserialize(
        InputStreamFormatter<utf8>& formatter,
        IEntityInterface& interf,
        DocumentTypeId docType)
    {
        auto docId = interf.CreateDocument(docType, nullptr);

            // Parse the input file, and send the result to the given entity interface
            // we expect only a list of entities in the root (no attributes)
        using Blob = InputStreamFormatter<utf8>::Blob;
        for (;;) {
            switch (formatter.PeekNext()) {
            case Blob::BeginElement:
                DeserializeEntity(formatter, interf, docId);
                break;

            case Blob::None: return; // end of file

            default:
                Throw(FormatException("Unexpected blob while deserializing entities", formatter.GetLocation()));
            }
        }
    }

}

