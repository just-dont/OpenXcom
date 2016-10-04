/*
 * Copyright 2010-2015 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenXcom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenXcom.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef OPENXCOM_SCRIPT_H
#define	OPENXCOM_SCRIPT_H

#include <map>
#include <limits>
#include <vector>
#include <string>
#include <yaml-cpp/yaml.h>
#include <SDL_stdinc.h>

#include "HelperMeta.h"
#include "Logger.h"
#include "Exception.h"


namespace OpenXcom
{
class Surface;

class ScriptGlobal;
class ScriptParserBase;
class ScriptParserEventsBase;
class ScriptContainerBase;
class ScriptContainerEventsBase;

class ParserWriter;
class SelectedToken;
class ScriptWorkerBase;
class ScriptWorkerBlit;
template<typename, typename...> class ScriptWorker;
template<typename, typename> struct ScriptTag;
template<typename, typename> class ScriptValues;

namespace helper
{

template<typename T>
struct ArgSelector;

}

constexpr int ScriptMaxOut = 4;
constexpr int ScriptMaxArg = 16;
constexpr int ScriptMaxReg = 64*sizeof(void*);

////////////////////////////////////////////////////////////
//					enum definitions
////////////////////////////////////////////////////////////

/**
 * Script execution cunter.
 */
enum class ProgPos : size_t
{
	Unknown = (size_t)-1,
	Start = 0,
};

inline ProgPos& operator+=(ProgPos& pos, int offset)
{
	pos = static_cast<ProgPos>(static_cast<size_t>(pos) + offset);
	return pos;
}
inline ProgPos& operator++(ProgPos& pos)
{
	pos += 1;
	return pos;
}
inline ProgPos operator++(ProgPos& pos, int)
{
	ProgPos old = pos;
	++pos;
	return old;
}

/**
 * Args special types.
 */
enum ArgSpecEnum : Uint8
{
	ArgSpecNone = 0x0,
	ArgSpecReg = 0x1,
	ArgSpecVar = 0x3,
	ArgSpecPtr = 0x4,
	ArgSpecPtrE = 0xC,
	ArgSpecSize = 0x10,
};
constexpr ArgSpecEnum operator|(ArgSpecEnum a, ArgSpecEnum b)
{
	return static_cast<ArgSpecEnum>(static_cast<Uint8>(a) | static_cast<Uint8>(b));
}
constexpr ArgSpecEnum operator&(ArgSpecEnum a, ArgSpecEnum b)
{
	return static_cast<ArgSpecEnum>(static_cast<Uint8>(a) & static_cast<Uint8>(b));
}
constexpr ArgSpecEnum operator^(ArgSpecEnum a, ArgSpecEnum b)
{
	return static_cast<ArgSpecEnum>(static_cast<Uint8>(a) ^ static_cast<Uint8>(b));
}

/**
 * Args types.
 */
enum ArgEnum : Uint8
{
	ArgInvalid = ArgSpecSize * 0,

	ArgNull = ArgSpecSize * 1,
	ArgInt = ArgSpecSize * 2,
	ArgLabel = ArgSpecSize * 3,
	ArgMax = ArgSpecSize * 4,
};

/**
 * Next avaiable value for arg type.
 */
constexpr ArgEnum ArgNext(ArgEnum arg)
{
	return static_cast<ArgEnum>(static_cast<Uint8>(arg) + static_cast<Uint8>(ArgSpecSize));
}

/**
 * Base version of argument type.
 */
constexpr ArgEnum ArgBase(ArgEnum arg)
{
	return static_cast<ArgEnum>((static_cast<Uint8>(arg) & ~(static_cast<Uint8>(ArgSpecSize) - 1)));
}
/**
 * Specialized version of argument type.
 */
constexpr ArgEnum ArgSpecAdd(ArgEnum arg, ArgSpecEnum spec)
{
	return ArgBase(arg) != ArgInvalid ? static_cast<ArgEnum>(static_cast<Uint8>(arg) | static_cast<Uint8>(spec)) : arg;
}
/**
 * Remove specialization from argument type.
 */
constexpr ArgEnum ArgSpecRemove(ArgEnum arg, ArgSpecEnum spec)
{
	return ArgBase(arg) != ArgInvalid ? static_cast<ArgEnum>(static_cast<Uint8>(arg) & ~static_cast<Uint8>(spec)) : arg;
}
/**
 * Test if argumet type is register (readonly or writeable).
 */
constexpr bool ArgIsReg(ArgEnum arg)
{
	return (static_cast<Uint8>(arg) & static_cast<Uint8>(ArgSpecReg)) == static_cast<Uint8>(ArgSpecReg);
}
/**
 * Test if argumet type is variable (writeable register).
 */
constexpr bool ArgIsVar(ArgEnum arg)
{
	return (static_cast<Uint8>(arg) & static_cast<Uint8>(ArgSpecVar)) == static_cast<Uint8>(ArgSpecVar);
}
/**
 * Test if argumet type is pointer.
 */
constexpr bool ArgIsPtr(ArgEnum arg)
{
	return (static_cast<Uint8>(arg) & static_cast<Uint8>(ArgSpecPtr)) == static_cast<Uint8>(ArgSpecPtr);
}
/**
 * Test if argumet type is editable pointer.
 */
constexpr bool ArgIsPtrE(ArgEnum arg)
{
	return (static_cast<Uint8>(arg) & static_cast<Uint8>(ArgSpecPtrE)) == static_cast<Uint8>(ArgSpecPtrE);
}
/**
 * Compatibility betwean operation argument type and variable type. Greater numbers mean bigger comatibility.
 * @param argType Type of operation argument.
 * @param varType Type of variable/value we try pass to operation.
 * @return Zero if incompatible, 255 if both types are same.
 */
constexpr int ArgCompatible(ArgEnum argType, ArgEnum varType, size_t overloadSize)
{
	return
		argType == ArgInvalid ? 0 :
		ArgIsVar(argType) && argType != varType ? 0 :
		ArgBase(argType) != ArgBase(varType) ? 0 :
		ArgIsReg(argType) != ArgIsReg(varType) ? 0 :
		ArgIsPtr(argType) != ArgIsPtr(varType) ? 0 :
		ArgIsPtrE(argType) && ArgIsPtr(varType) ? 0 :
			255 - (ArgIsPtrE(argType) != ArgIsPtrE(varType) ? 128 : 0) - (ArgIsVar(argType) != ArgIsVar(varType) ? 64 : 0) - (overloadSize > 8 ? 8 : overloadSize);
}

/**
 * Avaiable regs.
 */
enum RegEnum : Uint8
{
	RegInvaild = (Uint8)-1,

	RegMax = 0*sizeof(int),
};

/**
 * Return value from script operation.
 */
enum RetEnum : Uint8
{
	RetContinue = 0,
	RetEnd = 1,
	RetError = 2,
};

////////////////////////////////////////////////////////////
//				containers definitions
////////////////////////////////////////////////////////////

using FuncCommon = RetEnum (*)(ScriptWorkerBase&, const Uint8*, ProgPos&);

/**
 * Common base of script execution.
 */
class ScriptContainerBase
{
	friend class ParserWriter;
	std::vector<Uint8> _proc;

public:
	/// Constructor.
	ScriptContainerBase() = default;
	/// Copy constructor.
	ScriptContainerBase(const ScriptContainerBase&) = delete;
	/// Move constructor.
	ScriptContainerBase(ScriptContainerBase&&) = default;

	/// Destructor.
	~ScriptContainerBase() = default;

	/// Copy.
	ScriptContainerBase &operator=(const ScriptContainerBase&) = delete;
	/// Move.
	ScriptContainerBase &operator=(ScriptContainerBase&&) = default;

	/// Test if is any script there.
	explicit operator bool() const
	{
		return !_proc.empty();
	}

	/// Get pointer to proc data.
	const Uint8* data() const
	{
		return *this ? _proc.data() : nullptr;
	}
};

/**
 * Strong typed script.
 */
template<typename Parent, typename... Args>
class ScriptContainer : public ScriptContainerBase
{
public:
	/// Load data string from YAML.
	void load(const std::string& type, const YAML::Node& node, const Parent& parent)
	{
		parent.parseNode(*this, type, node);
	}
};

/**
 * Common base of typed script with events.
 */
class ScriptContainerEventsBase
{
	friend class ScriptParserEventsBase;
	ScriptContainerBase _current;
	const ScriptContainerBase* _events;

public:
	/// Test if is any script there.
	explicit operator bool() const
	{
		return true;
	}

	/// Get pointer to proc data.
	const Uint8* data() const
	{
		return _current.data();
	}
	/// Get pointer to proc data.
	const ScriptContainerBase* dataEvents() const
	{
		return _events;
	}
};

/**
 * Strong typed script with events.
 */
template<typename Parent, typename... Args>
class ScriptContainerEvents : public ScriptContainerEventsBase
{
public:
	/// Load data string form YAML.
	void load(const std::string& type, const YAML::Node& node, const Parent& parent)
	{
		parent.parseNode(*this, type, node);
	}
};

////////////////////////////////////////////////////////////
//					worker definition
////////////////////////////////////////////////////////////

namespace helper
{

template<int I, typename Tuple, bool valid = ((size_t)I < std::tuple_size<Tuple>::value)>
struct GetTupleImpl
{
	using type = typename std::tuple_element<I, Tuple>::type;
	static type get(Tuple& t) { return std::get<I>(t); }
};

template<int I, typename Tuple>
struct GetTupleImpl<I, Tuple, false>
{
	using type = void;
	static type get(Tuple& t) { }
};

template<int I, typename Tuple>
using GetTupleType = typename GetTupleImpl<I, Tuple>::type;

template<int I, typename Tuple>
GetTupleType<I, Tuple> GetTupleValue(Tuple& t) { return GetTupleImpl<I, Tuple>::get(t); }

template<typename T>
struct TypeInfoImpl
{
	using t1 = typename std::decay<T>::type;
	using t2 = typename std::remove_pointer<t1>::type;
	using t3 = typename std::decay<t2>::type;

	static constexpr bool isRef = std::is_reference<T>::value;
	static constexpr bool isOutput = isRef && !std::is_const<T>::value;
	static constexpr bool isPtr = std::is_pointer<t1>::value;
	static constexpr bool isEditable = isPtr && !std::is_const<t2>::value;

	static constexpr size_t size = std::is_pod<t3>::value ? sizeof(t3) : 0;

	static_assert(size || isPtr, "Type need to be POD to be used as reg or const value.");
};
} //namespace helper

/**
 * Raw memory used by scripts.
 */
template<int size>
using ScriptRawMemory = typename std::aligned_storage<size, alignof(void*)>::type;

/**
 * Script output and input aguments.
 */
template<typename... OutputArgs>
struct ScriptOutputArgs
{
	std::tuple<helper::Decay<OutputArgs>...> data;

	/// Constructor.
	ScriptOutputArgs(const helper::Decay<OutputArgs>&... args) : data{ args... }
	{

	}

	/// Getter for first element.
	auto getFirst() -> helper::GetTupleType<0, decltype(data)> { return helper::GetTupleValue<0>(data); }
	/// Getter for second element.
	auto getSecond() -> helper::GetTupleType<1, decltype(data)> { return helper::GetTupleValue<1>(data); }
	/// Getter for third element.
	auto getThird() -> helper::GetTupleType<2, decltype(data)> { return helper::GetTupleValue<2>(data); }
};

/**
 * Class execute scripts and strore its data.
 */
class ScriptWorkerBase
{
	ScriptRawMemory<ScriptMaxReg> reg;

	static constexpr int RegSet = 1;
	static constexpr int RegNone = 0;
	static constexpr int RegGet = -1;


	template<typename>
	using SetAllRegs = helper::PosTag<RegSet>;

	template<typename T>
	using GetWritableRegs = helper::PosTag<helper::TypeInfoImpl<T>::isOutput ? RegGet : RegNone>;

	template<typename T>
	using SetReadonlyRegs = helper::PosTag<!helper::TypeInfoImpl<T>::isOutput ? RegSet : RegNone>;

	template<int BaseOffset, int I, typename... Args, typename T>
	void forRegImplLoop(helper::PosTag<RegSet>, const T& arg)
	{
		using CurrentType =helper::Decay<typename std::tuple_element<I, T>::type>;
		constexpr int CurrentOffset = BaseOffset + offset<Args...>(I);

		ref<CurrentType>(CurrentOffset) = std::get<I>(arg);
	}
	template<int BaseOffset, int I, typename... Args, typename T>
	void forRegImplLoop(helper::PosTag<RegNone>, const T& arg)
	{
		// nothing
	}
	template<int BaseOffset, int I, typename... Args, typename T>
	void forRegImplLoop(helper::PosTag<RegGet>, T& arg)
	{
		using CurrentType = helper::Decay<typename std::tuple_element<I, T>::type>;
		constexpr int CurrentOffset = BaseOffset + offset<Args...>(I);

		std::get<I>(arg) = ref<CurrentType>(CurrentOffset);
	}

	template<int BaseOffset, template<typename> class Filter, typename... Args, typename T, int... I>
	void forRegImpl(T&& arg, helper::ListTag<I...>)
	{
		(void)helper::DummySeq
		{
			(forRegImplLoop<BaseOffset, I, Args...>(Filter<Args>{}, std::forward<T>(arg)), 0)...,
		};
	}

	template<int BaseOffset, template<typename> class Filter, typename... Args, typename T>
	void forReg(T&& arg)
	{
		forRegImpl<BaseOffset, Filter, Args...>(std::forward<T>(arg), helper::MakeListTag<sizeof...(Args)>{});
	}

	/// Count offset.
	template<typename First, typename Second, typename... Rest>
	static constexpr int offset(int i)
	{
		return (i > 0 ? sizeof(First) : 0) + (i > 1 ? offset<Second, Rest...>(i - 1) : 0);
	}
	/// Final function of counting offset.
	template<typename First>
	static constexpr int offset(int i)
	{
		return (i > 0 ? sizeof(First) : 0);
	}

	template<typename... Args>
	static constexpr int offsetOutput(helper::TypeTag<ScriptOutputArgs<Args...>>)
	{
		return offset<Args...>(sizeof...(Args));
	}

protected:
	/// Update values in script.
	template<typename Output, typename... Args>
	void updateBase(Args... args)
	{
		memset(&reg, 0, ScriptMaxReg);
		forReg<offsetOutput(helper::TypeTag<Output>{}), SetAllRegs, Args...>(std::tie(args...));
	}

	template<typename... Args>
	void set(const ScriptOutputArgs<Args...>& arg)
	{
		forReg<0, SetAllRegs, Args...>(arg.data);
	}
	template<typename... Args>
	void get(ScriptOutputArgs<Args...>& arg)
	{
		forReg<0, GetWritableRegs, Args...>(arg.data);
	}
	template<typename... Args>
	void reset(const ScriptOutputArgs<Args...>& arg)
	{
		forReg<0, SetReadonlyRegs, Args...>(arg.data);
	}

	/// Call script.
	void executeBase(const Uint8* proc);

public:
	/// Default constructor.
	ScriptWorkerBase()
	{

	}

	/// Get value from reg.
	template<typename T>
	T& ref(size_t offset)
	{
		return *reinterpret_cast<typename std::decay<T>::type*>(reinterpret_cast<char*>(&reg) + offset);
	}
	/// Get value from proc vector.
	template<typename T>
	const T& const_val(const Uint8 *ptr, size_t offset = 0)
	{
		return *reinterpret_cast<const T*>(ptr + offset);
	}
};

/**
 * Strong typed script executor base template.
 */
template<typename Output, typename... Args>
class ScriptWorker;

/**
 * Strong typed script executor.
 */
template<typename... OutputArgs, typename... Args>
class ScriptWorker<ScriptOutputArgs<OutputArgs...>, Args...> : public ScriptWorkerBase
{
public:
	/// Type of output value from script.
	using Output = ScriptOutputArgs<OutputArgs...>;

	/// Default constructor.
	ScriptWorker(Args... args) : ScriptWorkerBase()
	{
		updateBase<Output>(args...);
	}

	/// Execute standard script.
	template<typename Parent>
	void execute(const ScriptContainer<Parent, Args...>& c, Output& arg)
	{
		static_assert(std::is_same<typename Parent::Output, Output>::value, "Incompatible script output type");

		set(arg);
		executeBase(c.data());
		get(arg);
	}

	/// Execute standard script with global events.
	template<typename Parent>
	void execute(const ScriptContainerEvents<Parent, Args...>& c, Output& arg)
	{
		static_assert(std::is_same<typename Parent::Output, Output>::value, "Incompatible script output type");

		set(arg);
		auto ptr = c.dataEvents();
		if (ptr)
		{
			while (*ptr)
			{
				reset(arg);
				executeBase(ptr->data());
				++ptr;
			}
			++ptr;
		}
		reset(arg);
		executeBase(c.data());
		if (ptr)
		{
			while (*ptr)
			{
				reset(arg);
				executeBase(ptr->data());
				++ptr;
			}
		}
		get(arg);
	}
};

/**
 * Strong typed blit script executor.
 */
class ScriptWorkerBlit : public ScriptWorkerBase
{
	/// Current script set in worker.
	const Uint8* _proc;

public:
	/// Type of output value from script.
	using Output = ScriptOutputArgs<int&, int>;

	/// Default constructor.
	ScriptWorkerBlit() : ScriptWorkerBase(), _proc(nullptr)
	{

	}

	/// Update data from container script.
	template<typename Parent, typename... Args>
	void update(const ScriptContainer<Parent, Args...>& c, helper::Decay<Args>... args)
	{
		static_assert(std::is_same<typename Parent::Output, Output>::value, "Incompatible script output type");
		if (c)
		{
			_proc = c.data();
			updateBase<Output>(args...);
		}
		else
		{
			clear();
		}
	}

	/// Programmable bliting using script.
	void executeBlit(Surface* src, Surface* dest, int x, int y, int shade, bool half = false);

	/// Clear all worker data.
	void clear()
	{
		_proc = nullptr;
	}
};

////////////////////////////////////////////////////////////
//					objects ranges
////////////////////////////////////////////////////////////

/**
 * Range of values.
 */
template<typename T>
class ScriptRange
{
protected:
	using ptr = const T*;

	/// Pointer pointing place of first elemet.
	ptr _begin;
	/// pointer pointing place past of last elemet.
	ptr _end;

public:
	/// Defualt constructor.
	ScriptRange() : _begin{ nullptr }, _end{ nullptr }
	{

	}
	/// Constructor.
	ScriptRange(ptr b, ptr e) : _begin{ b }, _end{ e }
	{

	}

	/// Begining of string range.
	ptr begin() const
	{
		return _begin;
	}
	/// End of string range.
	ptr end() const
	{
		return _end;
	}
	/// Size of string range.
	size_t size() const
	{
		return _end - _begin;
	}
	/// Bool operator.
	explicit operator bool() const
	{
		return _begin != _end;
	}
};

/**
 * Symbol in script.
 */
class ScriptRef : public ScriptRange<char>
{
public:
	/// Default constructor.
	ScriptRef() = default;

	/// Copy constructor.
	ScriptRef(const ScriptRef&) = default;

	/// Constructor from pointer.
	explicit ScriptRef(ptr p) : ScriptRange{ p , p + strlen(p) }
	{

	}
	/// Constructor from range of pointers.
	ScriptRef(ptr b, ptr e) : ScriptRange{ b, e }
	{

	}

	/// Find first orrucace of character in string range.
	size_t find(char c) const
	{
		for (auto &curr : *this)
		{
			if (curr == c)
			{
				return &curr - _begin;
			}
		}
		return std::string::npos;
	}

	/// Return sub range of current range.
	ScriptRef substr(size_t p, size_t s = std::string::npos) const
	{
		const size_t totalSize = _end - _begin;
		if (p >= totalSize)
		{
			return ScriptRef{ };
		}

		const auto b = _begin + p;
		if (s > totalSize - p)
		{
			return ScriptRef{ b, _end };
		}
		else
		{
			return ScriptRef{ b, b + s };
		}
	}

	/// Create string based on current range.
	std::string toString() const
	{
		return *this ? std::string(_begin, _end) : std::string{ };
	}

	/// Create temporary ref based on script.
	static ScriptRef tempFrom(const std::string& s)
	{
		return { s.data(), s.data() + s.size() };
	}

	/// Compare two ranges.
	static int compare(ScriptRef a, ScriptRef b)
	{
		const auto size_a = a.size();
		const auto size_b = b.size();
		if (size_a == size_b)
		{
			return memcmp(a._begin, b._begin, size_a);
		}
		else if (size_a < size_b)
		{
			return -1;
		}
		else
		{
			return 1;
		}
	}
	/// Equal operator.
	bool operator==(const ScriptRef& s) const
	{
		return compare(*this, s) == 0;
	}
	/// Notequal operator.
	bool operator!=(const ScriptRef& s) const
	{
		return compare(*this, s) != 0;
	}
	/// Less operator.
	bool operator<(const ScriptRef& s) const
	{
		return compare(*this, s) < 0;
	}
};

////////////////////////////////////////////////////////////
//					parser definitions
////////////////////////////////////////////////////////////

/**
 * Struct storing script type data.
 */
struct ScriptTypeData
{
	ScriptRef name;
	ArgEnum type;
	size_t size;
};

/**
 * Struct storing value used by script.
 */
struct ScriptValueData
{
	ScriptRawMemory<sizeof(void*)> data;
	ArgEnum type = ArgInvalid;
	Uint8 size = 0;

	/// Copy constructor.
	template<typename T>
	inline ScriptValueData(const T& t);
	/// Copy constructor.
	inline ScriptValueData(const ScriptValueData& t);
	/// Default constructor.
	inline ScriptValueData() { }

	/// Assign operator.
	template<typename T>
	inline ScriptValueData& operator=(const T& t);
	/// Assign operator.
	inline ScriptValueData& operator=(const ScriptValueData& t);

	/// Test if value have have selected type.
	template<typename T>
	inline bool isValueType() const;
	/// Get current stored value.
	template<typename T>
	inline const T& getValue() const;
};

/**
 * Struct used to store named definition used by script.
 */
struct ScriptRefData
{
	ScriptRef name;
	ArgEnum type = ArgInvalid;
	ScriptValueData value;

	/// Default constructor.
	ScriptRefData() { }
	/// Constructor.
	ScriptRefData(ScriptRef n, ArgEnum t) : name{ n }, type{ t } {  }
	/// Constructor.
	ScriptRefData(ScriptRef n, ArgEnum t, ScriptValueData v) : name{ n }, type{ t }, value{ v } {  }

	/// Get true if this vaild reference.
	explicit operator bool() const
	{
		return type != ArgInvalid;
	}

	template<typename T>
	bool isValueType() const
	{
		return value.isValueType<T>();
	}
	/// Get current stored value.
	template<typename T>
	const T& getValue() const
	{
		return value.getValue<T>();
	}
	/// Get current stored value if have that type or defulat value otherwise.
	template<typename T>
	const T& getValueOrDefulat(const T& def) const
	{
		return value.isValueType<T>() ? value.getValue<T>() : def;
	}
};

/**
 * Struct storing avaliable operation to scripts.
 */
struct ScriptProcData
{
	using argFunc = int (*)(ParserWriter& ph, const ScriptRefData* begin, const ScriptRefData* end);
	using getFunc = FuncCommon (*)(int version);
	using parserFunc = bool (*)(const ScriptProcData& spd, ParserWriter& ph, const ScriptRefData* begin, const ScriptRefData* end);
	using overloadFunc = int (*)(const ScriptProcData& spd, const ScriptRefData* begin, const ScriptRefData* end);

	ScriptRef name;

	overloadFunc overload;
	ScriptRange<ScriptRange<ArgEnum>> overloadArg;

	parserFunc parser;
	argFunc parserArg;
	getFunc parserGet;

	bool operator()(ParserWriter& ph, const ScriptRefData* begin, const ScriptRefData* end) const
	{
		return parser(*this, ph, begin, end);
	}
};

/**
 * Common base of script parser.
 */
class ScriptParserBase
{
	ScriptGlobal* _shared;
	Uint8 _regUsed;
	Uint8 _regOutSize;
	ScriptRef _regOutName[ScriptMaxOut];
	std::string _name;
	std::string _defaultScript;
	std::vector<std::vector<char>> _strings;
	std::vector<ScriptTypeData> _typeList;
	std::vector<ScriptProcData> _procList;
	std::vector<ScriptRefData> _refList;

protected:
	template<typename Z>
	struct ArgName
	{
		ArgName(const char *n) : name{ n } { }

		const char *name;
	};

	template<typename First, typename... Rest>
	void addRegImpl(bool writable, ArgName<First>& n, Rest&... t)
	{
		addTypeImpl(helper::TypeTag<helper::Decay<First>>{});
		addScriptReg(n.name, ScriptParserBase::getArgType<First>(), writable, helper::TypeInfoImpl<First>::isOutput);
		addRegImpl(writable, t...);
	}
	void addRegImpl(bool writable)
	{
		//end loop
	}

	/// Function for SFINAE, type need to have ScriptRegister function.
	template<typename First, typename = decltype(&First::ScriptRegister)>
	void addTypeImpl(helper::TypeTag<First*>)
	{
		registerPointerType<First>();
	}
	/// Function for SFINAE, type need to have ScriptRegister function.
	template<typename First, typename = decltype(&First::ScriptRegister)>
	void addTypeImpl(helper::TypeTag<const First*>)
	{
		registerPointerType<First>();
	}
	/// Function for SFINAE, type need to have ScriptRegister function.
	template<typename First, typename Index, typename = decltype(&First::ScriptRegister)>
	void addTypeImpl(helper::TypeTag<ScriptTag<First, Index>>)
	{
		registerPointerType<First>();
	}
	/// Basic version.
	template<typename First>
	void addTypeImpl(helper::TypeTag<First>)
	{
		//nothing to do for rest
	}

	static ArgEnum registeTypeImplNextValue()
	{
		static ArgEnum curr = ArgMax;
		ArgEnum old = curr;
		curr = ArgNext(curr);
		return old;
	}
	template<typename T>
	static ArgEnum registeTypeImpl()
	{
		if (std::is_same<T, int>::value)
		{
			return ArgInt;
		}
		else if (std::is_same<T, std::nullptr_t>::value)
		{
			return ArgNull;
		}
		else if (std::is_same<T, ProgPos>::value)
		{
			return ArgLabel;
		}
		else
		{
			static ArgEnum curr = registeTypeImplNextValue();
			return curr;
		}
	}

	/// Default constructor.
	ScriptParserBase(ScriptGlobal* shared, const std::string& name);
	/// Destructor.
	~ScriptParserBase();

	/// Common typeless part of parsing string.
	bool parseBase(ScriptContainerBase& scr, const std::string& parentName, const std::string& code) const;

	/// Prase string and return new script.
	void parseNode(ScriptContainerBase& container, const std::string& type, const YAML::Node& node) const;

	/// Test if name is free.
	bool haveNameRef(const std::string& s) const;
	/// Add new name that can be used in data lists.
	ScriptRef addNameRef(const std::string& s);

	/// Add name for custom parameter.
	void addScriptReg(const std::string& s, ArgEnum type, bool writableReg, bool outputReg);
	/// Add parsing fuction.
	void addParserBase(const std::string& s, ScriptProcData::overloadFunc overload, ScriptRange<ScriptRange<ArgEnum>> overloadArg, ScriptProcData::parserFunc parser, ScriptProcData::argFunc parserArg, ScriptProcData::getFunc parserGet);
	/// Add new type impl.
	void addTypeBase(const std::string& s, ArgEnum type, size_t size);
	/// Test if type was added impl.
	bool haveTypeBase(ArgEnum type);
	/// Set defulat script for type.
	void setDefault(const std::string& s) { _defaultScript = s; }

public:
	/// Register type to get run time value representing it.
	template<typename T>
	static ArgEnum getArgType()
	{
		using info = helper::TypeInfoImpl<T>;
		using t3 = typename info::t3;

		auto spec = ArgSpecNone;

		if (info::isRef) spec = spec | ArgSpecVar;
		if (info::isPtr) spec = spec | ArgSpecPtr;
		if (info::isEditable) spec = spec | ArgSpecPtrE;

		return ArgSpecAdd(registeTypeImpl<t3>(), spec);
	}
	/// Add const value.
	void addConst(const std::string& s, ScriptValueData i);
	/// Update const value.
	void updateConst(const std::string& s, ScriptValueData i);
	/// Add line parsing function.
	template<typename T>
	void addParser(const std::string& s)
	{
		addParserBase(s, nullptr, T::overloadType(), nullptr, &T::parse, &T::getDynamic);
	}
	/// Test if type was already added.
	template<typename T>
	bool haveType()
	{
		return haveTypeBase(getArgType<T>());
	}
	/// Add new type.
	template<typename T>
	void addType(const std::string& s)
	{
		using info = helper::TypeInfoImpl<T>;
		using t3 = typename info::t3;

		addTypeBase(s, registeTypeImpl<t3>(), info::size);
	}
	/// Regised type in parser.
	template<typename P>
	void registerPointerType()
	{
		if (!haveType<P*>())
		{
			addType<P*>(P::ScriptName);
			P::ScriptRegister(this);
		}
	}

	/// Load global data from YAML.
	virtual void load(const YAML::Node& node);

	/// Show all script informations.
	void logScriptMetadata(bool haveEvents) const;

	/// Get name of script.
	const std::string& getName() const { return _name; }
	/// Get defulat script.
	const std::string& getDefault() const { return _defaultScript; }

	/// Get number of param.
	Uint8 getParamSize() const { return _regOutSize; }
	/// Get param data.
	const ScriptRefData* getParamData(Uint8 i) const { return getRef(_regOutName[i]); }

	/// Get name of type.
	ScriptRef getTypeName(ArgEnum type) const;
	/// Get full name of type.
	std::string getTypePrefix(ArgEnum type) const;
	/// Get type data.
	const ScriptTypeData* getType(ArgEnum type) const;
	/// Get type data.
	const ScriptTypeData* getType(ScriptRef name, ScriptRef postfix = {}) const;
	/// Get function data.
	ScriptRange<ScriptProcData> getProc(ScriptRef name, ScriptRef postfix = {}) const;
	/// Get arguments data.
	const ScriptRefData* getRef(ScriptRef name, ScriptRef postfix = {}) const;
	/// Get script shared data.
	ScriptGlobal* getGlobal() { return _shared; }
	/// Get script shared data.
	const ScriptGlobal* getGlobal() const { return _shared; }
};

/**
 * Copy constructor from pod type.
 */
template<typename T>
inline ScriptValueData::ScriptValueData(const T& t)
{
	static_assert(sizeof(T) <= sizeof(data), "Value have too big size!");
	type = ScriptParserBase::getArgType<T>();
	size = sizeof(T);
	memcpy(&data, &t, sizeof(T));
}

/**
 * Copy constructor.
 */
inline ScriptValueData::ScriptValueData(const ScriptValueData& t)
{
	*this = t;
}

/**
 * Assign operator from pod type.
 */
template<typename T>
inline ScriptValueData& ScriptValueData::operator=(const T& t)
{
	*this = ScriptValueData{ t };
	return *this;
}

/**
 * Assign operator.
 */
inline ScriptValueData& ScriptValueData::operator=(const ScriptValueData& t)
{
	type = t.type;
	size = t.size;
	memcpy(&data, &t.data, sizeof(data));
	return *this;
}

/**
 * Test if value have have selected type.
 */
template<typename T>
inline bool ScriptValueData::isValueType() const
{
	return type == ScriptParserBase::getArgType<T>();
}

/**
 * Get current stored value.
 */
template<typename T>
inline const T& ScriptValueData::getValue() const
{
	if (!isValueType<T>())
	{
		throw Exception("Invalid cast of value");
	}
	return *reinterpret_cast<const T*>(&data);
}

/**
 * Base template of strong typed parser.
 */
template<typename OutputPar, typename... Args>
class ScriptParser
{
public:
	using Container = ScriptContainerEvents<ScriptParser, Args...>;
	using Output = OutputPar;
	using Worker = ScriptWorker<Output, Args...>;

	static_assert(helper::StaticError<ScriptParser>::value, "Invalid parameters to template");
};

/**
 * Strong typed parser.
 */
template<typename... OutputArgs, typename... Args>
class ScriptParser<ScriptOutputArgs<OutputArgs...>, Args...> : public ScriptParserBase
{
public:
	using Container = ScriptContainer<ScriptParser, Args...>;
	using Output = ScriptOutputArgs<OutputArgs...>;
	using Worker = ScriptWorker<Output, Args...>;
	friend Container;

	/// Constructor.
	ScriptParser(ScriptGlobal* shared, const std::string& name, ArgName<OutputArgs>... argOutputNames, ArgName<Args>... argNames) : ScriptParserBase(shared, name)
	{
		addRegImpl(true, argOutputNames...);
		addRegImpl(false, argNames...);
	}
};

/**
 * Common base for strong typed event parser.
 */
class ScriptParserEventsBase : public ScriptParserBase
{
	constexpr static size_t EventsMax = 64;
	constexpr static size_t OffsetScale = 100;
	constexpr static size_t OffsetMax = 100 * OffsetScale;

	struct EventData
	{
		int offset;
		ScriptContainerBase script;
	};

	/// Final list of events.
	std::vector<ScriptContainerBase> _events;
	/// Meta data of events.
	std::vector<EventData> _eventsData;

protected:
	/// Prase string and return new script.
	void parseNode(ScriptContainerEventsBase& container, const std::string& type, const YAML::Node& node) const;

public:
	/// Constructor.
	ScriptParserEventsBase(ScriptGlobal* shared, const std::string& name);

	/// Load global data from YAML.
	virtual void load(const YAML::Node& node) override;
	/// Get pointer to events.
	const ScriptContainerBase* getEvents() const;
	/// Relese event data.
	std::vector<ScriptContainerBase> releseEvents();
};

/**
 * Base template of strong typed event parser.
 */
template<typename OutputPar, typename... Args>
class ScriptParserEvents
{
public:
	using Container = ScriptContainerEvents<ScriptParserEvents, Args...>;
	using Output = OutputPar;
	using Worker = ScriptWorker<Output, Args...>;

	static_assert(helper::StaticError<ScriptParserEvents>::value, "Invalid parameters to template");
};

/**
 * Strong typed event parser.
 */
template<typename... OutputArgs, typename... Args>
class ScriptParserEvents<ScriptOutputArgs<OutputArgs...>, Args...> : public ScriptParserEventsBase
{
public:
	using Container = ScriptContainerEvents<ScriptParserEvents, Args...>;
	using Output = ScriptOutputArgs<OutputArgs...>;
	using Worker = ScriptWorker<Output, Args...>;
	friend Container;

	/// Constructor.
	ScriptParserEvents(ScriptGlobal* shared, const std::string& name, ArgName<OutputArgs>... argOutputNames, ArgName<Args>... argNames) : ScriptParserEventsBase(shared, name)
	{
		addRegImpl(true, argOutputNames...);
		addRegImpl(false, argNames...);
	}
};

////////////////////////////////////////////////////////////
//					tags definitions
////////////////////////////////////////////////////////////

/**
 * Strong typed tag.
 */
template<typename T, typename I = Uint8>
struct ScriptTag
{
	static_assert(!std::numeric_limits<I>::is_signed, "Type should be unsigned");
	static_assert(sizeof(I) <= sizeof(size_t), "Type need be smaller than size_t");

	using Parent = T;

	/// Index that identify value in ScriptValues.
	I index;

	/// Get value.
	constexpr size_t get() const { return static_cast<size_t>(index); }
	/// Test if tag have valid value.
	constexpr explicit operator bool() const { return this->index; }
	/// Equal operator.
	constexpr bool operator==(ScriptTag t)
	{
		return index == t.index;
	}
	/// Notequal operator.
	constexpr bool operator!=(ScriptTag t)
	{
		return !(*this == t);
	}

	/// Get run time value for type.
	static ArgEnum type() { return ScriptParserBase::getArgType<ScriptTag<T, I>>(); }
	/// Test if value can be used.
	static constexpr bool isValid(size_t i) { return i && i <= limit(); }
	/// Fake constructor.
	static constexpr ScriptTag make(size_t i) { return { static_cast<I>(i) }; }
	/// Max supprted value.
	static constexpr size_t limit() { return static_cast<size_t>(std::numeric_limits<I>::max()); }
	/// Null value.
	static constexpr ScriptTag getNullTag() { return make(0); }
};

/**
 * Global data shared by all scripts.
 */
class ScriptGlobal
{
protected:
	using LoadFunc = void (*)(const ScriptGlobal*, int&, const YAML::Node&);
	using SaveFunc = void (*)(const ScriptGlobal*, const int&, YAML::Node&);
	using CrateFunc = ScriptValueData (*)(size_t i);

	friend class ScriptValuesBase;

	struct TagValueType
	{
		ScriptRef name;
		LoadFunc load;
		SaveFunc save;
	};
	struct TagValueData
	{
		ScriptRef name;
		size_t valueType;
	};
	struct TagData
	{
		ScriptRef name;
		size_t limit;
		CrateFunc crate;
		std::vector<TagValueData> values;
	};

	template<typename ThisType, void (ThisType::* LoadValue)(int&, const YAML::Node&) const>
	static void loadHelper(const ScriptGlobal* base, int& value, const YAML::Node& node)
	{
		(static_cast<const ThisType*>(base)->*LoadValue)(value, node);
	}
	template<typename ThisType, void (ThisType::* SaveValue)(const int&, YAML::Node&) const>
	static void saveHelper(const ScriptGlobal* base, const int& value, YAML::Node& node)
	{
		(static_cast<const ThisType*>(base)->*SaveValue)(value, node);
	}

	void addTagValueTypeBase(const std::string& name, LoadFunc loadFunc, SaveFunc saveFunc)
	{
		_tagValueTypes.push_back(TagValueType{ addNameRef(name), loadFunc, saveFunc });
	}
	template<typename ThisType, void (ThisType::* LoadValue)(int&, const YAML::Node&) const, void (ThisType::* SaveValue)(const int&, YAML::Node&) const>
	void addTagValueType(const std::string& name)
	{
		static_assert(std::is_base_of<ScriptGlobal, ThisType>::value, "Type must be derived");
		addTagValueTypeBase(name, &loadHelper<ThisType, LoadValue>, &saveHelper<ThisType, SaveValue>);
	}
private:
	std::vector<std::vector<char>> _strings;
	std::vector<std::vector<ScriptContainerBase>> _events;
	std::map<std::string, ScriptParserBase*> _parserNames;
	std::vector<ScriptParserEventsBase*> _parserEvents;
	std::map<ArgEnum, TagData> _tagNames;
	std::vector<TagValueType> _tagValueTypes;
	std::vector<ScriptRefData> _refList;

	/// Get tag value.
	size_t getTag(ArgEnum type, ScriptRef s) const;
	/// Get data of tag value.
	TagValueData getTagValueData(ArgEnum type, size_t i) const;
	/// Get tag value type data.
	TagValueType getTagValueTypeData(size_t valueType) const;
	/// Get tag value type id.
	size_t getTagValueTypeId(ScriptRef s) const;
	/// Add new tag name.
	size_t addTag(ArgEnum type, ScriptRef s, size_t valueType);
	/// Add new name ref.
	ScriptRef addNameRef(const std::string& s);

public:
	/// Default constructor.
	ScriptGlobal();
	/// Destructor.
	virtual ~ScriptGlobal();

	/// Store parser.
	void pushParser(ScriptParserBase* parser);
	/// Store parser.
	void pushParser(ScriptParserEventsBase* parser);

	/// Add new const value.
	void addConst(const std::string& name, ScriptValueData i);
	/// Update const value.
	void updateConst(const std::string& name, ScriptValueData i);

	/// Get global ref data.
	const ScriptRefData* getRef(ScriptRef name, ScriptRef postfix = {}) const;

	/// Get tag based on it name.
	template<typename Tag>
	Tag getTag(ScriptRef s) const
	{
		return Tag::make(getTag(Tag::type(), s));
	}
	/// Get tag based on it name.
	template<typename Tag>
	Tag getTag(const std::string& s) const
	{
		return getTag<Tag>(ScriptRef::tempFrom(s));
	}
	/// Add new tag name.
	template<typename Tag>
	Tag addTag(const std::string& s, const std::string& valueTypeName)
	{
		return Tag::make(addTag(Tag::type(), addNameRef(s), getTagValueTypeId(ScriptRef::tempFrom(valueTypeName))));
	}
	/// Add new type of tag.
	template<typename Tag>
	void addTagType()
	{
		if (_tagNames.find(Tag::type()) == _tagNames.end())
		{
			_tagNames.insert(
				std::make_pair(
					Tag::type(),
					TagData
					{
						ScriptRef{ Tag::Parent::ScriptName },
						Tag::limit(),
						[](size_t i) { return ScriptValueData{ Tag::make(i) }; },
						std::vector<TagValueData>{},
					}
				)
			);
		}
	}

	/// Prepare for loading data.
	virtual void beginLoad();
	/// Finishing loading data.
	virtual void endLoad();

	/// Load global data from YAML.
	void load(const YAML::Node& node);
};

/**
 * Colection of values for script usage.
 */
class ScriptValuesBase
{
	/// Vector with all avaiable values for script.
	std::vector<int> values;

protected:
	/// Set value.
	void setBase(size_t t, int i);
	/// Get value.
	int getBase(size_t t) const;
	/// Load values from yaml file.
	void loadBase(const YAML::Node &node, const ScriptGlobal* shared, ArgEnum type);
	/// Save values to yaml file.
	void saveBase(YAML::Node &node, const ScriptGlobal* shared, ArgEnum type) const;
};

/**
 * Strong typed colection of values for srcipt.
 */
template<typename T, typename I = Uint8>
class ScriptValues : ScriptValuesBase
{
public:
	using Tag = ScriptTag<T, I>;

	/// Load values from yaml file.
	void load(const YAML::Node &node, const ScriptGlobal* shared)
	{
		loadBase(node, shared, Tag::type());
	}
	/// Save values to yaml file.
	void save(YAML::Node &node, const ScriptGlobal* shared) const
	{
		saveBase(node, shared, Tag::type());;
	}

	/// Get value.
	int get(Tag t) const
	{
		return getBase(t.get());
	}
	/// Set value.
	void set(Tag t, int i)
	{
		return setBase(t.get(), i);
	}
};

} //namespace OpenXcom

#endif	/* OPENXCOM_SCRIPT_H */

