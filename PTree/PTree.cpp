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

struct iProperty
{
	enum ePropertyType
	{
		ePTString = 0,
		ePTInt,
		ePTVector3,
		ePTCollection
	};

	virtual ~iProperty() {}

	virtual const char* GetName() const = 0;
	virtual ePropertyType GetType() const = 0;

	template <typename T_>
	virtual T_ GetValue() const = 0;
	template <typename T_>
	virtual void SetValue(const T_& v) = 0;
};

struct iPropertyIterator
{
	virtual ~iPropertyIterator() = 0;

	virtual bool Next() = 0;
	virtual struct iPVisitorElement& Get() = 0;
};

struct iIterableProperties
{
	typedef std::unique_ptr<iPropertyIterator> tUniquePIterator;

	virtual ~iIterableProperties() = 0;

	virtual tUniquePIterator CreateIterator() const = 0;
};

struct iPropertyVisitor
{
	virtual ~iPropertyVisitor() {}

	template <iProperty::ePropertyType T_>
	virtual void Visit(iProperty& p) = 0;
};

struct iPVisitorElement
{
	virtual ~iPVisitorElement() {}

	virtual void Accept(iPropertyVisitor& visitor) = 0;
};

class cProperty : public iProperty, public iPVisitorElement
{
public:
	cProperty(const char* name, int& r) : Name(name), RInt(&r), Type(iProperty::ePTInt) {}
	cProperty(const char* name, Vector3& r) : Name(name), RVec3(&r), Type(iProperty::ePTVector3) {}
	cProperty(const char* name, std::string& r) : Name(name), RStr(&r), Type(iProperty::ePTString) {}
	cProperty(const char* name, const iIterableProperties& r) : Name(name), RCollection(&r), Type(iProperty::ePTCollection) {}
	virtual ~cProperty() {}

	// iProperty:
	virtual const char* GetName() const override { return Name.c_str(); }
	virtual ePropertyType GetType() const override { return Type; }

	template <typename T_>
	virtual T_ GetValue() const override;
	template <typename T_>
	virtual void SetValue(const T_& v) override;
	// iProperty.

	// iPVisitorElement:
	virtual void Accept(iPropertyVisitor& visitor) override;
	// iPVisitorElement.

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
	assert(Type == iProperty::ePTInt);
	return *RInt;
}

template<> void cProperty::SetValue(const int& v)
{
	assert(Type == iProperty::ePTInt);
	*RInt = v;
}

template<> const char* cProperty::GetValue() const
{
	assert(Type == iProperty::ePTString);
	return RStr->c_str();
}
template<> void cProperty::SetValue(const char* const& v)
{
	assert(Type == iProperty::ePTString);
	*RStr = v;
}

template<> const Vector3& cProperty::GetValue() const
{
	assert(Type == iProperty::ePTVector3);
	return *RVec3;
}
template<> void cProperty::SetValue(const Vector3& v)
{
	assert(Type == iProperty::ePTVector3);
	*RVec3 = v;
}

template<> const iIterableProperties& cProperty::GetValue() const
{
	assert(Type == iProperty::ePTCollection);
	return *RCollection;
}
template<> void cProperty::SetValue(const iIterableProperties& v)
{
	assert(Type == iProperty::ePTCollection);
	RCollection = &v;
}

void cProperty::Accept(iPropertyVisitor& visitor)
{
	switch (Type)
	{
	case iProperty::ePTInt: visitor.Visit<ePTInt>(*this); break;
	case iProperty::ePTString: visitor.Visit<ePTString>(*this); break;
	case iProperty::ePTVector3: visitor.Visit<ePTVector3>(*this); break;
	case iProperty::ePTCollection: visitor.Visit<ePTCollection>(*this); break;
	default: assert(false);
	}
}

class cXMLSerializer : public iPropertyVisitor
{
public:
	cXMLSerializer(tBoostPTree& pt, iPropertyIterator& iter);

	// iPropertyVisitor:
	template <iProperty::ePropertyType T_>
	virtual void Visit(iProperty& p) override;
	// iPropertyVisitor.

private:
	tBoostPTree& PT;
};

cXMLSerializer::cXMLSerializer(tBoostPTree& pt, iPropertyIterator& iter)
	: PT(pt)
{
	while (iter.Next())
		iter.Get().Accept(*this);
}

template<> void cXMLSerializer::Visit<iProperty::ePTInt>(iProperty& p)
{
	PT.put<int>(p.GetName(), p.GetValue<int>());
}

template<> void cXMLSerializer::Visit<iProperty::ePTString>(iProperty& p)
{
	PT.put<std::string>(p.GetName(), p.GetValue<const char*>());
}

template<> void cXMLSerializer::Visit<iProperty::ePTVector3>(iProperty& p)
{
	const Vector3& v = p.GetValue<const Vector3&>();
	tBoostPTree pt;
	pt.put<float>("x", v.X);
	pt.put<float>("y", v.Y);
	pt.put<float>("z", v.Z);
	PT.add(p.GetName(), pt);
}

template<> void cXMLSerializer::Visit<iProperty::ePTCollection>(iProperty& p)
{
	tBoostPTree pt;
	cXMLSerializer(pt, *p.GetValue<const iIterableProperties&>().CreateIterator());
	PT.add(p.GetName(), pt);
}

class cXMLDeserializer : public iPropertyVisitor
{
public:
	cXMLDeserializer(tBoostPTree& pt, iPropertyIterator& iter);

	// iPropertyVisitor:
	template <iProperty::ePropertyType T_>
	virtual void Visit(iProperty& p) override;
	// iPropertyVisitor.

private:
	tBoostPTree& PT;
};

cXMLDeserializer::cXMLDeserializer(tBoostPTree& pt, iPropertyIterator& iter)
	: PT(pt)
{
	while (iter.Next())
		iter.Get().Accept(*this);
}

template<> void cXMLDeserializer::Visit<iProperty::ePTInt>(iProperty& p)
{
	p.SetValue(PT.get<int>(p.GetName()));
}

template<> void cXMLDeserializer::Visit<iProperty::ePTString>(iProperty& p)
{
	p.SetValue(PT.get<std::string>(p.GetName()));
}

template<> void cXMLDeserializer::Visit<iProperty::ePTVector3>(iProperty& p)
{
	tBoostPTree pt = PT.get<tBoostPTree>(p.GetName());
	p.SetValue(Vector3(pt.get<float>("x"), pt.get<float>("y"), pt.get<float>("z")));
}

template<> void cXMLDeserializer::Visit<iProperty::ePTCollection>(iProperty& p)
{
	tBoostPTree pt = PT.get<tBoostPTree>(p.GetName());
	cXMLDeserializer(pt, *p.GetValue<const iIterableProperties&>().CreateIterator());
}

class cPropertiesList : public iIterableProperties
{
public:
	typedef std::unique_ptr<cProperty> rProperty;

public:
	cPropertiesList();

	// iIterableProperties:
	virtual tUniquePIterator CreateIterator() const override { return std::unique_ptr<iPropertyIterator>(new cIterator(Properties)); }
	// iIterableProperties.

	void Register(rProperty p) { Properties.push_back(p); }

private:
	typedef std::vector<rProperty> tProperties;

	class cIterator : public iPropertyIterator
	{
	public:
		cIterator(const tProperties& properties) : Properties(properties), Index(-1) {}
		// iPropertyIterator:
		virtual bool Next() override { return ++Index < Properties.size(); }
		virtual iPVisitorElement& Get() override { assert(Index < Properties.size()); return *Properties[Index].get(); }
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
	void Register(iBaseProperties& object) { Registery.push_back(object); }
	void SaveXML(const char* file);
	void LoadXML(const char* file) {}

private:
	typedef std::vector<iBaseProperties&> tRegistry;
	tRegistry Registery;
};

class cTestActor : cBaseProperties
{
public:
	cTestActor()
		: Name("Termagoyf")
		, Health(100)
		, Pos(100.f, 50.f, 0.f)
	{
		PList.Register(std::unique_ptr<cProperty>(new cProperty("Name", Name)));
		PList.Register(std::unique_ptr<cProperty>(new cProperty("Health", Health)));
		PList.Register(std::unique_ptr<cProperty>(new cProperty("Position", Pos)));
	}
	~cTestActor() {}

private:
	int Health;
	std::string Name;
	Vector3 Pos;
};

int _tmain(int argc, _TCHAR* argv[])
{
	return 0;
}