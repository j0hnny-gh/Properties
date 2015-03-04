// PTree.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <string>
#include <assert.h>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

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
		ePTVector3,
		ePTCollection
	};
public:
	cProperty(const char* name, int& r) : Name(name), RInt(&r), Type(cProperty::ePTInt) {}
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
	typedef cProperty* rProperty;

public:
	~cPropertiesList()
	{
		for (auto& p : Properties)
			delete p;
	}

	// iIterableProperties:
	virtual rUniquePIterator CreateIterator() const override { return rUniquePIterator(new cIterator(Properties)); }
	// iIterableProperties.

	void Register(rProperty p) { Properties.push_back(p); }

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

struct iBaseProperties
{
	virtual ~iBaseProperties() {}

	virtual iIterableProperties& GetProperties() = 0;
};

class cBaseProperties : public iBaseProperties
{
public:
	// iBaseProperties:
	iIterableProperties& GetProperties() { return PList; }
	// iBaseProperties.

protected:
	cPropertiesList PList;
};

class cPropertiesSystem
{
public:
	void Register(iBaseProperties& object) { Registery.push_back(&object); }
	void SaveXML(const char* file);
	void LoadXML(const char* file);

private:
	typedef std::vector<iBaseProperties*> tRegistry;
	tRegistry Registery;
};

void cPropertiesSystem::SaveXML(const char* file)
{
	tBoostPTree pt;
	for (auto& obj : Registery)
		cXMLSerializer saver(pt, *obj->GetProperties().CreateIterator());
	write_xml(file, pt);
}

void cPropertiesSystem::LoadXML(const char* file)
{
	tBoostPTree pt;
	read_xml(file, pt);
	for (auto& obj : Registery)
		cXMLDeserializer loader(pt, *obj->GetProperties().CreateIterator());
}

class cTestActor : public cBaseProperties
{
public:
	cTestActor()
		: Name("Termogoyf")
		, Health(100)
		, Pos(100.f, 50.f, 0.f)
	{
		PList.Register(cPropertiesList::rProperty(new cProperty("Name", Name)));
		PList.Register(cPropertiesList::rProperty(new cProperty("Health", Health)));
		PList.Register(cPropertiesList::rProperty(new cProperty("Position", Pos)));
	}
	~cTestActor() {}

private:
	int Health;
	std::string Name;
	Vector3 Pos;
};

int _tmain(int argc, _TCHAR* argv[])
{
	cPropertiesSystem propSystem;
	cTestActor actor;
	propSystem.Register(actor);
	propSystem.LoadXML("termogoyf");
	propSystem.SaveXML("termogoyf.xml");
	return 0;
}