// PTree.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <string>
#include <assert.h>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

typedef unsigned int uint;

struct Vector3
{
	Vector3() : X(0.f), Y(0.f), Z(0.f) {}
	Vector3(float x, float y, float z) : X(x), Y(y), Z(z) {}

	float X;
	float Y;
	float Z;
};

typedef boost::property_tree::ptree tBoostPTree;

struct iPropertyIterator
{
	virtual ~iPropertyIterator() {}

	virtual bool Next() = 0;
	virtual class cProperty& Get() = 0;
};

struct iIterableProperties
{
	typedef std::unique_ptr<iPropertyIterator> rUniquePIterator;

	virtual ~iIterableProperties() {}

	virtual rUniquePIterator CreateIterator() const = 0;
};

class cProperty
{
public:
	enum ePropertyType
	{
		ePTString = 0,
		ePTInt,
		ePTUInt,
		ePTVector3,
		ePTCollection
	};
public:
	cProperty(const char* name, int& r) : Name(name), RInt(&r), Type(cProperty::ePTInt) {}
	cProperty(const char* name, uint& r) : Name(name), RUInt(&r), Type(cProperty::ePTUInt) {}
	cProperty(const char* name, Vector3& r) : Name(name), RVec3(&r), Type(cProperty::ePTVector3) {}
	cProperty(const char* name, std::string& r) : Name(name), RStr(&r), Type(cProperty::ePTString) {}
	cProperty(const char* name, const iIterableProperties& r) : Name(name), RCollection(&r), Type(cProperty::ePTCollection) {}

	const char* GetName() const { return Name.c_str(); }
	ePropertyType GetType() const { return Type; }

	template <typename T_>
	T_ GetValue() const;
	template <typename T_>
	void SetValue(const T_& v);

	template<class V_>
	void Accept(V_& visitor);

private:
	const ePropertyType Type;
	const std::string Name;
	union
	{
		int* RInt;
		uint* RUInt;
		Vector3* RVec3;
		std::string* RStr;
		const iIterableProperties* RCollection;
	};
};

template<> int cProperty::GetValue() const
{
	assert(Type == cProperty::ePTInt);
	return *RInt;
}

template<> void cProperty::SetValue(const int& v)
{
	assert(Type == cProperty::ePTInt);
	*RInt = v;
}

template<> uint cProperty::GetValue() const
{
	assert(Type == cProperty::ePTUInt);
	return *RUInt;
}

template<> void cProperty::SetValue(const uint& v)
{
	assert(Type == cProperty::ePTUInt);
	*RUInt = v;
}

template<> const char* cProperty::GetValue() const
{
	assert(Type == cProperty::ePTString);
	return RStr->c_str();
}

template<> void cProperty::SetValue(const char* const& v)
{
	assert(Type == cProperty::ePTString);
	*RStr = v;
}

template<> const Vector3& cProperty::GetValue() const
{
	assert(Type == cProperty::ePTVector3);
	return *RVec3;
}

template<> void cProperty::SetValue(const Vector3& v)
{
	assert(Type == cProperty::ePTVector3);
	*RVec3 = v;
}

template<> const iIterableProperties& cProperty::GetValue() const
{
	assert(Type == cProperty::ePTCollection);
	return *RCollection;
}

template<> void cProperty::SetValue(const iIterableProperties& v)
{
	assert(Type == cProperty::ePTCollection);
	RCollection = &v;
}

template<class V_>
void cProperty::Accept(V_& visitor)
{
	switch (Type)
	{
	case cProperty::ePTInt: visitor.Visit<ePTInt>(*this); break;
	case cProperty::ePTUInt: visitor.Visit<ePTUInt>(*this); break;
	case cProperty::ePTString: visitor.Visit<ePTString>(*this); break;
	case cProperty::ePTVector3: visitor.Visit<ePTVector3>(*this); break;
	case cProperty::ePTCollection: visitor.Visit<ePTCollection>(*this); break;
	default: assert(false);
	}
}

class cXMLSerializer
{
public:
	cXMLSerializer(tBoostPTree& pt, iPropertyIterator& iter);

	template <cProperty::ePropertyType T_>
	void Visit(cProperty& p);

private:
	tBoostPTree& PT;
};

cXMLSerializer::cXMLSerializer(tBoostPTree& pt, iPropertyIterator& iter)
	: PT(pt)
{
	while (iter.Next())
		iter.Get().Accept(*this);
}

template<> void cXMLSerializer::Visit<cProperty::ePTInt>(cProperty& p)
{
	PT.put<int>(p.GetName(), p.GetValue<int>());
}

template<> void cXMLSerializer::Visit<cProperty::ePTUInt>(cProperty& p)
{
	PT.put<uint>(p.GetName(), p.GetValue<uint>());
}

template<> void cXMLSerializer::Visit<cProperty::ePTString>(cProperty& p)
{
	PT.put<std::string>(p.GetName(), p.GetValue<const char*>());
}

template<> void cXMLSerializer::Visit<cProperty::ePTVector3>(cProperty& p)
{
	const Vector3& v = p.GetValue<const Vector3&>();
	tBoostPTree pt;
	pt.put<float>("x", v.X);
	pt.put<float>("y", v.Y);
	pt.put<float>("z", v.Z);
	PT.add_child(p.GetName(), pt);
}

template<> void cXMLSerializer::Visit<cProperty::ePTCollection>(cProperty& p)
{
	tBoostPTree pt;
	cXMLSerializer(pt, *p.GetValue<const iIterableProperties&>().CreateIterator());
	PT.add_child(p.GetName(), pt);
}

class cXMLDeserializer
{
public:
	cXMLDeserializer(tBoostPTree& pt, iPropertyIterator& iter);

	template <cProperty::ePropertyType T_>
	void Visit(cProperty& p);

private:
	tBoostPTree& PT;
};

cXMLDeserializer::cXMLDeserializer(tBoostPTree& pt, iPropertyIterator& iter)
	: PT(pt)
{
	while (iter.Next())
		iter.Get().Accept(*this);
}

template<> void cXMLDeserializer::Visit<cProperty::ePTInt>(cProperty& p)
{
	p.SetValue(PT.get<int>(p.GetName()));
}

template<> void cXMLDeserializer::Visit<cProperty::ePTUInt>(cProperty& p)
{
	p.SetValue(PT.get<uint>(p.GetName()));
}

template<> void cXMLDeserializer::Visit<cProperty::ePTString>(cProperty& p)
{
	p.SetValue(PT.get<std::string>(p.GetName()).c_str());
}

template<> void cXMLDeserializer::Visit<cProperty::ePTVector3>(cProperty& p)
{
	tBoostPTree pt = PT.get_child(p.GetName());
	p.SetValue(Vector3(pt.get<float>("x"), pt.get<float>("y"), pt.get<float>("z")));
}

template<> void cXMLDeserializer::Visit<cProperty::ePTCollection>(cProperty& p)
{
	tBoostPTree pt = PT.get_child(p.GetName());
	cXMLDeserializer(pt, *p.GetValue<const iIterableProperties&>().CreateIterator());
}

class cPropertiesList : public iIterableProperties
{
public:
	typedef std::unique_ptr<cProperty> rProperty;

public:
	// iIterableProperties:
	virtual rUniquePIterator CreateIterator() const override { return rUniquePIterator(new cIterator(Properties)); }
	// iIterableProperties.

	void Register(rProperty p) { Properties.push_back(std::move(p)); }

private:
	typedef std::vector<rProperty> tProperties;

	class cIterator : public iPropertyIterator
	{
	public:
		cIterator(const tProperties& properties) : Properties(properties), Index(-1) {}

		// iPropertyIterator:
		virtual bool Next() override { return ++Index < (int)Properties.size(); }
		virtual cProperty& Get() override { assert(Index < (int)Properties.size()); return *Properties[Index]; }
		// iPropertyIterator.

	private:
		const tProperties& Properties;
		int Index;
	};

private:
	tProperties Properties;
};

struct iBaseObject
{
	virtual ~iBaseObject() {}

	virtual iIterableProperties& GetProperties() = 0;
	virtual uint GetID() const = 0;
	virtual const char* GetObjectType() const = 0;
};

class cBaseObject : public iBaseObject
{
public:
	cBaseObject(uint id, const char* type)
		: ID(id)
		, ObjectType(type)
	{
		PList.Register(cPropertiesList::rProperty(new cProperty("ID", ID)));
	}

	// iBaseProperties:
	iIterableProperties& GetProperties() { return PList; }
	virtual uint GetID() const { return ID; }
	virtual const char* GetObjectType() const { return ObjectType.c_str(); }
	// iBaseProperties.

protected:
	uint ID;
	std::string ObjectType;
	cPropertiesList PList;
};

class cObjectSystem
{
public:
	struct iFactory
	{
		virtual ~iFactory() {}

		virtual iBaseObject* Create(uint id, const char* name) const = 0;
		virtual const char* GetType() const = 0;
	};

	typedef std::unique_ptr<iFactory> rFactory;

	template <class C_>
	class cFactory : public iFactory
	{
	public:
		cFactory(const char* type) : Type(type) {}

		// iFactory:
		virtual iBaseObject* Create(uint id, const char* name) const override { return new C_(id); }
		virtual const char* GetType() const override { return Type.c_str(); }
		// iFactory.

	private:
		const std::string Type;
	};

public:
	cObjectSystem() : NextID(0) {}

	void SaveXML(const char* file);
	void LoadXML(const char* file);

	template <class C_>
	C_* Create(const char* name)
	{
		if (const iFactory* f = FindFactory(C_::SObjectType))
		{
			iBaseObject* object = f->Create(NextID++, name);
			RegisterObject(*object);
			return static_cast<C_*>(object);
		}
		return nullptr;
	}

	template <class C_>
	void Delete(C_*& object)
	{
		UnregisterObject(*object);
		object = nullptr;
	}

	void RegisterFactory(rFactory f)
	{
		Factories.push_back(std::move(f));
	}

private:
	void RegisterObject(iBaseObject& object) { Registery.push_back(&object); }
	void UnregisterObject(iBaseObject& object) { Registery.erase(std::find(Registery.begin(), Registery.end(), &object)); }

	const iFactory* FindFactory(const std::string& type) const
	{
		auto& it = std::find_if(Factories.begin(), Factories.end(), [&type](const rFactory& f) { return type == f->GetType(); });
		return (it != Factories.end()) ? it->get() : nullptr;
	}

	typedef std::vector<iBaseObject*> tRegistry;
	tRegistry Registery;

	typedef std::vector<rFactory> tFactories;
	tFactories Factories;

	uint NextID;
};

void cObjectSystem::SaveXML(const char* file)
{
	tBoostPTree pt;
	for (auto& obj : Registery)
	{
		tBoostPTree element;
		cXMLSerializer saver(element, *obj->GetProperties().CreateIterator());
		pt.add_child(obj->GetObjectType(), element);
	}
	write_xml(file, pt);
}

void cObjectSystem::LoadXML(const char* file)
{
	tBoostPTree pt;
	read_xml(file, pt);
	for (auto& element : pt.get_child(""))
	{
		if (const iFactory* f = FindFactory(element.first))
		{
			iBaseObject* object = f->Create(0, "");
			cXMLDeserializer loader(element.second, *object->GetProperties().CreateIterator());
			RegisterObject(*object);
		}
	}
}

class cActor : public cBaseObject
{
public:
	static const std::string SObjectType;

	cActor(uint id)
		: cBaseObject(id, SObjectType.c_str())
		, Name("Actor")
		, Health(100)
		, Pos(100.f, 50.f, 0.f)
	{
		PList.Register(cPropertiesList::rProperty(new cProperty("Name", Name)));
		PList.Register(cPropertiesList::rProperty(new cProperty("Health", Health)));
		PList.Register(cPropertiesList::rProperty(new cProperty("Position", Pos)));
	}
	~cActor() {}

private:
	int Health;
	std::string Name;
	Vector3 Pos;
};

const std::string cActor::SObjectType = "Actor";

int _tmain(int argc, _TCHAR* argv[])
{
	cObjectSystem ObjectSystem;
	ObjectSystem.RegisterFactory(cObjectSystem::rFactory(new cObjectSystem::cFactory<cActor>(cActor::SObjectType.c_str())));

	cActor* actor = ObjectSystem.Create<cActor>("Termogoyf");

	ObjectSystem.SaveXML("termogoyf.xml");

	ObjectSystem.LoadXML("termogoyf.xml");

	ObjectSystem.Delete<cActor>(actor);

	return 0;
}