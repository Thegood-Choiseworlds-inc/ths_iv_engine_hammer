#pragma once

//=========================================================

/*
    asbind.h - Copyright Christian Holmberg 2011

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

//=========================================================

/*

API

Binding a c++ class or struct can be done like this :

    class MyClass { ... };

    ASBind::Class<MyClass> myclass = ASBind::MyClass<MyClass>( asEngine );

Or :
    ASBind::Class<MyClass> myclass = ASBind::CreateClass<MyClass>( asEngine [, name ]  );

Or you can get already defined class like this :

    ASBind::Class<MyClass> myclass = ASBind::GetClass<MyClass>( asEngine, name );

You can bind a class either as a reference, POD or class type. You can reference to Angelscript
documentation on these. POD type is a type registered as asOBJ_VALUE | asOBJ_POD.
Class types are registered as asOBJ_VALUE | asCLASS_APP_CDAK.
You can choose the type as the second template argument which defaults to ASBind::class_ref
(reference type). Other values here are ASBind::class_pod (POD) and ASBind::class_class (complex class)

    ASBind::Class<MyPOD, ABind::class_pod>( asEngine, name );

And this needs to be declared in global scope/namespace

ASBIND_TYPE( MyClass, ClassName );

Binding methods to class can be done with .method(). There's two versions available,
one is to bind actual methods and the other is used to bind global c-like "methods"

    myclass.method( &MyClass::Function, "funcname" );

    void MyClass_Function( MyClass* self );

    myclass.method( MyClass_Function, "funcname", true );

The last argument on the second version is true if 'this' is the first of function parameters,
false if its the last of them.
You also have .constmethod() to explicitly bind a 'const' method (actual const methods get the
const qualifier automatically)

    myclass.constmethod( &MyClass::cFunction, "cfuncname" );
    myclass.constmethod( MyClass_cFunction, "cfuncname" );

Binding members is done with .member().

    myclass.member( &MyClass:Variable, "var" );

======

Factory functions are binded through .factory. These are for reference types if you want to create
instances of this type in scripts.
You can declare multiple factories with different prototypes

    MyClass* MyClass_Factory()
    {
        return new MyClass();
    }

    myclass.factory( MyClass_Factory );

Reference types need to declare functions for adding and releasing references. This is done
here like this.

    myclass.refs( &MyClass::AddRef, &MyClass:Release );

    myclass.refs( MyClass_AddRef, MyClass_Release );

Here's a utility struct you can derive from if you declare your own new classes to bind
    class ASBind::RefWrapper
    {
        void AddRef();
        void Release();
        int refcount;
    }

=====

Constructors are declared using the template arguments (which have to have void-return type).
Types that are registered as class_class need constructor/destructors.
You can declare multiple constructors with different prototypes (as for every function)

    myclass.constructor<void(int,int)>();

Or global function version (last parameter is once again true if 'this' is first in the function
argument list)

    void MyClass_Constructor( MyClass* self )
    {
        new( self ) MyClass();
    }

    myclass.constructor( MyClass_Constructor, true );


Destructors are just callbacks to void( MyClass* )
You can use a helper here
    myclass.destructor( ASBind::CallDestructor<MyClass> );

Or declare your own
    void Myclass_Destructor( MyClass* self )
    {
        self->~MyClass();
    }

    myclass.destructor( MyClass_Destructor );

You can define cast operations too, once again either as real methods or global functions

    string MyClass::toString() { ... return string; }
    string MyClass_toString(MyClass* self) { ... return string; }

    myclass.cast( &MyClass::toString );
    myclass.cast( MyClass_toString );

Note that any custom type you use has to be declared as a type before using them in any of these
bindings.

=====

Global functions and variables are binded through the Global object :

    ASBind::Global global;

    global.func( MyGlobalFunction, "globfunc" );
    global.var( MyGlobalVariable, "globvar" );

=====

Enumerations are done through Enum object

    enum { ENUM0, ENUM1, ENUM2 .. };

    ASBind::Enum e( "EnumType" );
    e.add( "ENUM0", ENUM0 );

Enumerations also have operator() that does the same as .add

    e( "ENUM0", ENUM0 );

=====

All functions here return a reference to 'this' so you can cascade them. Example for enum :

    ASBind::Enum( "EnumType" )
        .add( "ENUM0", ENUM0 )
        .add( "ENUM1", ENUM1 )
        .add( "ENUM2", ENUM2 );

And you can use the operator() in enum too for some funkyness

    ASBind::Enum( "EnumType" )( "ENUM0", ENUM0 )( "ENUM1", ENUM1 );

Chaining can be used on Class, Global and Enum.

=============

Fetching and calling script functions.
Lets say you have a 'main' function in the script like this

    void main() { ... }

You create a function pointer object and fetch it from the script in 2 ways:
You can fetch FunctionPtr's directly with the script function pointer (if you know it)

    ASBind::FunctionPtr<void()> mainPtr = ASBind::CreateFunctionPtr( scriptFunc, mainPtr );

Or by name (note the additional module parameter, needed to resolve the name to id)

    ASBind::FunctionPtr<void()> mainPtr = ASBind::CreateFunctionPtr( "main", asModule, mainPtr );

Note that the FunctionPtr has a matching function prototype as a template argument
and the instance is also passed as a reference to ASBind::CreateFunctionPtr. This is only a dummy parameter
used for template deduction.

Now you can call your function pointer syntactically the same way as regular functions :

    mainPtr();

Only thing here is that you are accessing operator() of the FunctionPtr which does some handy stuff
to set parameters and whatnot. FunctionPtr can also return values normally, except for custom types
only pointers and references are supported as-of-now (same goes for passing arguments i guess?)

*/

//=========================================================

// some configuration variables

// #define __DEBUG_DevMsg__

#include "tier0/dbg.h"
#include "tier1/utlstring.h"

#include <type_traits>

#if __has_include( "nameof.hpp" )
#include "nameof.hpp"
#endif

#include "angelscript.h"

namespace ASBind
{
	namespace detail
	{
		template <typename T, T offset, typename S>
		struct _integer_sequence2;

		template <typename _Ty, _Ty _Offset, _Ty... _Vals>
		struct _integer_sequence2<_Ty, _Offset, ::std::integer_sequence<_Ty, _Vals...>>
		{
			using type = ::std::integer_sequence<_Ty, ( _Offset + _Vals )...>;
		};

		template <class _Ty, _Ty _Size, _Ty _Offset>
		using make_integer_sequence2 = typename _integer_sequence2<_Ty, _Offset, ::std::make_integer_sequence<_Ty, _Size>>::type;
	}

//=========================================================

// asbind_typestring.h
// utilities to convert types to strings

#define ASBIND_TYPE( type, name ) \
	namespace ASBind {          \
	template<> \
	constexpr inline const char* typestr<type>() { return # name ; }   \
	}

#define ASBIND_ARRAY_TYPE( type, name ) \
	namespace ASBind {          \
	template<> \
	constexpr inline const char* typestr<type>() { return "array<" # name ">" ; }  \
	}

template <typename T>
constexpr const char* typestr() = delete;

template<>
constexpr inline const char* typestr<signed int>() { return "int"; }
template<>
constexpr inline const char* typestr<long>() { return "int"; }
template<>
constexpr inline const char* typestr<unsigned int>() { return "uint"; }
template<>
constexpr inline const char* typestr<unsigned long>() { return "uint"; }
template<>
constexpr inline const char* typestr<char>() { return "uint8"; }
template<>
constexpr inline const char* typestr<signed char>() { return "int8"; }
template<>
constexpr inline const char* typestr<unsigned char>() { return "uint8"; }
template<>
constexpr inline const char* typestr<signed short>() { return "int16"; }
template<>
constexpr inline const char* typestr<unsigned short>() { return "uint16"; }
template<>
constexpr inline const char* typestr<bool>() { return "bool"; }
template<>
constexpr inline const char* typestr<long long>() { return "int64"; }
template<>
constexpr inline const char* typestr<unsigned long long>() { return "uint64"; }

template<>
constexpr inline const char* typestr<float>() { return "float"; }
template<>
constexpr inline const char* typestr<double>() { return "double"; }

template<>
constexpr inline const char* typestr<void>() { return "void"; }

// custom types NEED to define static property
//  (string) typestr
template <typename T>
struct TypeStringProxy
{
	static CUtlString type( const char* name = nullptr )
	{
		CUtlString o = typestr<T>();
		if ( name && *name )
		{
			o += ' ';
			o += name;
		}
		return o;
	}
	static CUtlString return_type( const char* name = nullptr )
	{
		return type( name );
	}
};

template <typename T>
struct TypeStringProxy<T*>
{
	static CUtlString type( const char* name = nullptr )
	{
		CUtlString o = typestr<T>();
		if constexpr ( std::is_floating_point_v<T> || std::is_integral_v<T> || std::is_enum_v<T> )
			o += "&out";
		else
			o += "@";
		if ( name && *name )
		{
			o += ' ';
			o += name;
		}
		return o;
	}
	static CUtlString return_type( const char* name = nullptr )
	{
		CUtlString o = typestr<T>();
		if constexpr ( std::is_floating_point_v<T> || std::is_integral_v<T> || std::is_enum_v<T> )
			o += "&";
		else
			o += "@";
		if ( name && *name )
		{
			o += ' ';
			o += name;
		}
		return o;
	}
};

// FIXME: separate types and RETURN TYPES! return references dont have inout qualifiers!
template <typename T>
struct TypeStringProxy<T&>
{
	static CUtlString type( const char* name = nullptr )
	{
		CUtlString o = typestr<T>();
		o += "&out"; // FIXME should flag inout/out somehow..
		if ( name && *name )
		{
			o += ' ';
			o += name;
		}
		return o;
	}
	// no inout qualifiers for returned references
	static CUtlString return_type( const char* name = nullptr )
	{
		CUtlString o = typestr<T>();
		o += "&";  // FIXME should flag inout/out somehow..
		if ( name && *name )
		{
			o += ' ';
			o += name;
		}
		return o;
	}
};

template <typename T>
struct TypeStringProxy<T*&>
{
	static CUtlString type( const char* name = nullptr )
	{
		CUtlString o = typestr<T>();
		o += "@&inout";    // FIXME should flag inout/out somehow..
		if ( name && *name )
		{
			o += ' ';
			o += name;
		}
		return o;
	}
	// no inout qualifiers for returned references
	static CUtlString return_type( const char* name = nullptr )
	{
		CUtlString o = typestr<T>();
		o += "@&"; // FIXME should flag inout/out somehow..
		if ( name && *name )
		{
			o += ' ';
			o += name;
		}
		return o;
	}
};

template <typename T>
struct TypeStringProxy<const T>
{
	static CUtlString type( const char* name = nullptr )
	{
		CUtlString o = "const ";
		o += typestr<T>();
		if ( name && *name )
		{
			o += ' ';
			o += name;
		}
		return o;
	}
	static CUtlString return_type( const char* name = nullptr )
	{
		return type( name );
	}
};

template <typename T>
struct TypeStringProxy<const T*>
{
	static CUtlString type( const char* name = nullptr )
	{
		CUtlString o = "const ";
		o += typestr<T>();
		o += "@";
		if ( name && *name )
		{
			o += ' ';
			o += name;
		}
		return o;
	}
	static CUtlString return_type( const char* name = nullptr )
	{
		return type( name );
	}
};

// FIXME: separate types and RETURN TYPES! return references dont have inout qualifiers!
template <typename T>
struct TypeStringProxy<const T&>
{
	static CUtlString type( const char* name = nullptr )
	{
		CUtlString o = "const ";
		o += typestr<T>();
		o += "&in";
		if ( name && *name )
		{
			o += ' ';
			o += name;
		}
		return o;
	}
	// no inout qualifiers for returned references
	static CUtlString return_type( const char* name = nullptr )
	{
		CUtlString o = "const ";
		o += typestr<T>();
		o += "&";
		if ( name && *name )
		{
			o += ' ';
			o += name;
		}
		return o;
	}
};

template <typename T>
struct TypeStringProxy<const T*&>
{
	static CUtlString type( const char* name = nullptr )
	{
		CUtlString o = "const ";
		o += typestr<T>();
		o += "@&in";
		if ( name && *name )
		{
			o += ' ';
			o += name;
		}
		return o;
	}
	// no inout qualifiers for returned references
	static CUtlString return_type( const char* name = nullptr )
	{
		CUtlString o = "const ";
		o += typestr<T>();
		o += "@&";
		if ( name && *name )
		{
			o += ' ';
			o += name;
		}
		return o;
	}
};


//=================================================

// function string

template <typename R>
struct FunctionStringProxy
{
	static CUtlString type( const char* s ) = delete;
};

template <typename R, typename... A>
struct FunctionStringProxy<R( * )( A... )>
{
private:
	using _type = std::tuple<A...>;

	template <size_t... N>
	static CUtlString construct_big( std::index_sequence<N...> )
	{
		return TypeStringProxy<std::tuple_element_t<0, _type>>::type() + ( ( ", " + TypeStringProxy<std::tuple_element_t<N, _type>>::type() ) + ...);
	}

	template <size_t S>
	static CUtlString construct()
	{
		if constexpr ( S == 0 )
			return {};
		else if constexpr ( S == 1 )
			return TypeStringProxy<std::tuple_element_t<S - 1, _type>>::type();
		else
			return construct_big( detail::make_integer_sequence2<size_t, S - 1, 1>() );
	}
public:
	static CUtlString type( const char* s )
	{
		return TypeStringProxy<R>::return_type() + ' ' + s + " (" + construct<sizeof...( A )>() + ')';
	}
};

//========================================

// method string

template <typename T>
struct MethodStringProxy
{
	static CUtlString type( const char* s ) = delete;
};

//==

template <typename T, typename R, typename... A>
struct MethodStringProxy<R( T::* )( A... )>
{
private:
	using _type = std::tuple<A...>;

	template <size_t... N>
	static CUtlString construct_big( std::index_sequence<N...> )
	{
		return TypeStringProxy<std::tuple_element_t<0, _type>>::type() + ( ( ", " + TypeStringProxy<std::tuple_element_t<N, _type>>::type() ) + ...);
	}

	template <size_t S>
	static CUtlString construct()
	{
		if constexpr ( S == 0 )
			return {};
		else if constexpr ( S == 1 )
			return TypeStringProxy<std::tuple_element_t<S - 1, _type>>::type();
		else
			return construct_big( detail::make_integer_sequence2<size_t, S - 1, 1>() );
	}
public:
	static CUtlString type( const char* s )
	{
		// dont include T here
		return TypeStringProxy<R>::return_type() + ' ' + s + " (" + construct<sizeof...( A )>() + ')';
	}
};

template <typename T, typename R, typename... A>
struct MethodStringProxy<R( T::* )( A... ) const>
{
private:
	using _type = std::tuple<A...>;

	template <size_t... N>
	static CUtlString construct_big( std::index_sequence<N...> )
	{
		return TypeStringProxy<std::tuple_element_t<0, _type>>::type() + ( ( ", " + TypeStringProxy<std::tuple_element_t<N, _type>>::type() ) + ...);
	}

	template <size_t S>
	static CUtlString construct()
	{
		if constexpr ( S == 0 )
			return {};
		else if constexpr ( S == 1 )
			return TypeStringProxy<std::tuple_element_t<S - 1, _type>>::type();
		else
			return construct_big( detail::make_integer_sequence2<size_t, S - 1, 1>() );
	}
public:
	static CUtlString type( const char* s )
	{
		// dont include T here
		return TypeStringProxy<R>::return_type() + ' ' + s + " (" + construct<sizeof...( A )>() + ") const";
	}
};


//========================================

// actual function to convert type to string
template <typename T>
CUtlString TypeString( const char* name = nullptr )
{
	return TypeStringProxy<T>::type( name );
}

// actual function to convert the function type to string
template <typename F>
CUtlString FunctionString( const char* name = nullptr )
{
	using type = std::conditional_t<std::is_pointer_v<F>, F, std::add_pointer_t<F>>;
	return FunctionStringProxy<type>::type( name );
}

// actual function to convert the method type to string
template <typename F>
CUtlString MethodString( const char* name = nullptr )
{
	return MethodStringProxy<F>::type( name );
}

// actual function to convert the funcdef type to string
template <typename F>
CUtlString FuncdefString( const char* name = nullptr )
{
	using type = std::conditional_t<std::is_pointer_v<F>, F, std::add_pointer_t<F>>;
	return FunctionStringProxy<type>::type( name );
}

//=========================================================

// asbind_stripthis.h
// utility to strip 'this' from either start or the end of argument list
// and return a null function pointer that has the 'correct' prototype for a member

template <typename F>
struct StripThisProxy {};

template <typename R, typename... A>
struct StripThisProxy<R( * )( A... )>
{
	using Args = std::tuple<A...>;
private:
	template <size_t... S>
	static R( *helper( std::index_sequence<S...> ) )( std::tuple_element_t<S, Args>... ) { return nullptr; };
public:
	using func_in = decltype( helper( std::make_index_sequence<sizeof...( A )>() ) );
	using func_of = decltype( helper( detail::make_integer_sequence2<size_t, sizeof...( A ) - 1, sizeof...( A ) <= 1 ? 0 : 1>() ) );
	using func_ol = decltype( helper( std::make_index_sequence<sizeof...( A ) - 1>() ) );
};

//=========================================================

// asbind_func.h
// functor object to call script-function

// first define structs to get/set arguments (struct to partial-specialize)
template <typename T, size_t N>
struct SetArg
{
	template <typename T2 = T>
	FORCEINLINE static std::enable_if_t<std::is_enum_v<T2> && std::is_same_v<T, T2>> set( asIScriptContext* ctx, const T2& t ) { ctx->SetArgDWord( N, static_cast<unsigned int>( t ) ); }

	template <typename T2 = T>
	static std::enable_if_t<!(std::is_enum_v<T2> && std::is_same_v<T, T2>)> set( asIScriptContext* ctx, const T2& t ) = delete;
};

template <typename T>
struct GetArg
{
	static T get() = delete;
};

template <size_t N>
struct SetArg<signed int, N>
{
	FORCEINLINE static void set( asIScriptContext* ctx, signed int t ) { ctx->SetArgDWord( N, t ); }
};

template <size_t N>
struct SetArg<unsigned int, N>
{
	FORCEINLINE static void set( asIScriptContext* ctx, unsigned int t ) { ctx->SetArgDWord( N, t ); }
};

template <size_t N>
struct SetArg<signed short, N>
{
	FORCEINLINE static void set( asIScriptContext* ctx, signed short t ) { ctx->SetArgWord( N, t ); }
};

template <size_t N>
struct SetArg<unsigned short, N>
{
	FORCEINLINE static void set( asIScriptContext* ctx, unsigned short t ) { ctx->SetArgWord( N, t ); }
};

template <size_t N>
struct SetArg<char, N>
{
	FORCEINLINE static void set( asIScriptContext* ctx, char t ) { ctx->SetArgByte( N, t ); }
};

template <size_t N>
struct SetArg<signed char, N>
{
	FORCEINLINE static void set( asIScriptContext* ctx, signed char t ) { ctx->SetArgByte( N, t ); }
};

template <size_t N>
struct SetArg<unsigned char, N>
{
	FORCEINLINE static void set( asIScriptContext* ctx, unsigned char t ) { ctx->SetArgByte( N, t ); }
};

template <size_t N>
struct SetArg<float, N>
{
	FORCEINLINE static void set( asIScriptContext* ctx, float t ) { ctx->SetArgFloat( N, t ); }
};

template <size_t N>
struct SetArg<double, N>
{
	FORCEINLINE static void set( asIScriptContext* ctx, double t ) { ctx->SetArgDouble( N, t ); }
};

template <size_t N>
struct SetArg<long long, N>
{
	FORCEINLINE static void set( asIScriptContext* ctx, long long t ) { ctx->SetArgQWord( N, t ); }
};

template <size_t N>
struct SetArg<unsigned long long, N>
{
	FORCEINLINE static void set( asIScriptContext* ctx, unsigned long long t ) { ctx->SetArgQWord( N, t ); }
};

// bool FIXME: 32-bits on PowerPC
template <size_t N>
struct SetArg<bool, N>
{
	FORCEINLINE static void set( asIScriptContext* ctx, bool t ) { ctx->SetArgByte( N, (unsigned char)t ); }
};

// pointers and references
template <typename T, size_t N>
struct SetArg<T*, N>
{
	FORCEINLINE static void set( asIScriptContext* ctx, T* t ) { ctx->SetArgAddress( N, (void*)t ); }
};

template <typename T, size_t N>
struct SetArg<const T*, N>
{
	FORCEINLINE static void set( asIScriptContext* ctx, const T* t ) { ctx->SetArgAddress( N, (void*)t ); }
};

template <typename T, size_t N>
struct SetArg<T&, N>
{
	FORCEINLINE static void set( asIScriptContext* ctx, T& t ) { ctx->SetArgAddress( N, (void*)&t ); }
};

template <typename T, size_t N>
struct SetArg<const T&, N>
{
	FORCEINLINE static void set( asIScriptContext* ctx, const T& t ) { ctx->SetArgAddress( N, (void*)&t ); }
};

//==== RETURN
template<>
struct GetArg<signed int>
{
	FORCEINLINE static signed int get( asIScriptContext* ctx ) { return ctx->GetReturnDWord(); }
};

template<>
struct GetArg<unsigned int>
{
	FORCEINLINE static unsigned int get( asIScriptContext* ctx ) { return ctx->GetReturnDWord(); }
};

template<>
struct GetArg<signed short>
{
	FORCEINLINE static signed short get( asIScriptContext* ctx ) { return ctx->GetReturnWord(); }
};

template<>
struct GetArg<unsigned short>
{
	FORCEINLINE static unsigned short get( asIScriptContext* ctx ) { return ctx->GetReturnWord(); }
};

template<>
struct GetArg<char>
{
	FORCEINLINE static char get( asIScriptContext* ctx ) { return ctx->GetReturnByte(); }
};

template<>
struct GetArg<signed char>
{
	FORCEINLINE static signed char get( asIScriptContext* ctx ) { return ctx->GetReturnByte(); }
};

template<>
struct GetArg<unsigned char>
{
	FORCEINLINE static unsigned char get( asIScriptContext* ctx ) { return ctx->GetReturnByte(); }
};

template<>
struct GetArg<float>
{
	FORCEINLINE static float get( asIScriptContext* ctx ) { return ctx->GetReturnFloat(); }
};

template<>
struct GetArg<double>
{
	FORCEINLINE static double get( asIScriptContext* ctx ) { return ctx->GetReturnDouble(); }
};

template<>
struct GetArg<bool>
{
	FORCEINLINE static bool get( asIScriptContext* ctx ) { return ctx->GetReturnByte() != 0; }
};

template<>
struct GetArg<long long>
{
	FORCEINLINE static long long get( asIScriptContext* ctx ) { return ctx->GetReturnQWord(); }
};

template<>
struct GetArg<unsigned long long>
{
	FORCEINLINE static unsigned long long get( asIScriptContext* ctx ) { return ctx->GetReturnQWord(); }
};

// pointers and references
template <typename T>
struct GetArg<T*>
{
	FORCEINLINE static T* get( asIScriptContext* ctx ) { return static_cast<T*>( ctx->GetReturnAddress() ); }
};

template <typename T>
struct GetArg<const T*>
{
	FORCEINLINE static const T* get( asIScriptContext* ctx ) { return static_cast<T*>( ctx->GetReturnAddress() ); }
};

template <typename T>
struct GetArg<T&>
{
	FORCEINLINE static T& get( asIScriptContext* ctx ) { return *static_cast<T*>( ctx->GetReturnAddress() ); }
};

template <typename T>
struct GetArg<const T&>
{
	FORCEINLINE static const T& get( asIScriptContext* ctx ) { return *static_cast<T*>( ctx->GetReturnAddress() ); }
};

//====================

class FunctionPtrBase
{
	asIScriptFunction*	fptr;
protected:
	asIScriptContext*	ctx;
	asIScriptObject*	object;

private:
	bool				bfailed;
protected:

	FunctionPtrBase( asIScriptFunction* fptr )
		: fptr( fptr ), ctx( nullptr ), object( nullptr ), bfailed( false ) {}

public:
	void addref()
	{
		if ( fptr )
			fptr->AddRef();
	}

	void release()
	{
		if ( fptr )
		{
			asIScriptFunction* fptr_ = fptr;
			fptr = nullptr;
			fptr_->Release();
		}
	}

	bool isValid() const { return !!fptr; }
	bool failed() const { return bfailed; }

	const char* getName() const { return fptr ? fptr->GetName() : "#NULL#"; }
	asIScriptFunction* getPtr() const { return fptr; }
	asIScriptModule* getModule()
	{
		asIScriptFunction* f = fptr;
		while ( f && f->GetFuncType() == asFUNC_DELEGATE )
			f = f->GetDelegateFunction();
		return f ? f->GetModule() : nullptr;
	}

	void setContext( asIScriptContext* _ctx ) { ctx = _ctx; }
	void setObject( asIScriptObject* _object ) { object = _object; }

protected:
	// general calling function
	void precall()
	{
		if ( fptr )
		{
			ctx->Prepare( fptr );
			if ( object )
				ctx->SetObject( object );
		}
	}
	void call()
	{
		if ( !ctx )
			return;

		bfailed = false;
		int r = ctx->Execute();
		if ( r != asEXECUTION_FINISHED && r != asEXECUTION_SUSPENDED )
		{
			static const bool debug = CommandLine_Tier0()->FindParm( "-script_debug" ) != 0;
			if ( !debug )
			{
				const Color c{ 32, 32, 255, 0 };
				if ( r == asEXECUTION_EXCEPTION )
					ConColorMsg( c, "%s", GetExceptionInfo( ctx, true ).Get() );
				else
					ConColorMsg( c, "ASBind::FunctionPtrBase: Execute failed %d (name %s)\n", r, fptr->GetName() );
			}
			bfailed = true;
		}
	}
};

//=================

template <typename R>
class FunctionPtr : FunctionPtrBase
{
	FunctionPtr( asIScriptFunction* ) = delete;
	R operator()() = delete;
};

//==

template <typename R, typename... A>
class FunctionPtr<R( A... )> : public FunctionPtrBase
{
	using _type = std::tuple<A...>;

	template <size_t N, typename T1, typename... T>
	void apply( T1& t, T&... data )
	{
		SetArg<std::tuple_element_t<N, _type>, N>::set( ctx, t );
		if constexpr ( sizeof...( T ) > 0 )
			apply<N + 1, T...>( data... );
	}
public:
	using func_type = void ( * )( A... );
	FunctionPtr( asIScriptFunction* fptr = nullptr ) : FunctionPtrBase( fptr )
	{
#ifdef _DEBUG
		if ( fptr )
		{
			Assert( std::tuple_size_v<_type> == fptr->GetParamCount() );
		}
#endif // _DEBUG
	}

	R operator()( A... a )
	{
		precall();
		if constexpr ( sizeof...( A ) > 0 )
			apply<0>( a... );
		call();
		if constexpr ( !std::is_void_v<R> )
			return GetArg<R>::get( ctx );
	}
};

//==

// generator function to return the proxy functor object

template <typename F>
FunctionPtr<F> CreateFunctionPtr( const char* name, asIScriptModule* mod )
{
	asIScriptFunction* fptr = mod->GetFunctionByDecl( FunctionString<F>( name ).Get() );
	return FunctionPtr<F>( fptr );
}

//=========================================================

// asbind_class.h
// bind classes and its methods and members (and also global functions as methods)

enum ASClassType
{
	class_ref = 0,          // needs reference [ and opt. factory ] functions
	class_pod = 1,          // simple pod types
	class_class = 2,        // needs constructor/deconstructor, copyconstructor and assignment
	class_singleref = 3,    // there can only be 1 instance that cant be referenced (see AS docs)
	class_pod_allints = 4,  // same as class_pod but with all members being integers
	class_pod_allfloats = 5,// same as class_pod but with all members being floats
	class_nocount = 6,      // no reference counting, memory management is performed solely on app side
};

template <typename T, typename... Args>
auto CallCtor( T* t, Args... args ) -> std::enable_if_t<std::is_constructible_v<T, Args...>> { new( t ) T( args... ); }

template <typename T, typename A1>
struct CallCtorProxy
{
	static void* ctor() = delete;
};

template <typename T, typename... Args>
struct CallCtorProxy<T, void( Args... args )>
{
	static auto ctor() { return CallCtor<T, Args...>; }
};

class Interface
{
	asIScriptEngine*	engine;
	CUtlString			name;

public:
	Interface( asIScriptEngine* engine, const char* name ) : engine( engine ), name( name )
	{
		int id = engine->RegisterInterface( name );
		Assert( id >= 0 );
		if ( id < 0 )
			Error( "ASBind::Interface (%s) ctor failed %d", name, id );

#ifdef __DEBUG_DevMsg__
		DevMsg( "ASBind::Interface registered new interface %s\n", name );
#endif
	}

	Interface& method( const char* fullname )
	{
#ifdef __DEBUG_DevMsg__
		DevMsg( "%s::method %s\n", name.Get(), fullname );
#endif
		int _id = engine->RegisterInterfaceMethod( name.Get(), fullname );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Interface::method (%s::%s) RegisterInterfaceMethod failed %d", name.Get(), fullname, _id );
		return *this;
	}

	template <typename F>
	Interface& method( const char* fname )
	{
		using type = std::conditional_t<std::is_pointer_v<F>, F, std::add_pointer_t<F>>;
		CUtlString fullname = FunctionString<type>( fname );

#ifdef __DEBUG_DevMsg__
		DevMsg( "%s::method %s\n", name.Get(), fullname.Get() );
#endif
		int _id = engine->RegisterInterfaceMethod( name.Get(), fullname.Get() );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Interface::method (%s::%s) RegisterInterfaceMethod failed %d", name.Get(), fullname.Get(), _id );
		return *this;
	}

	template <typename F>
	Interface& constmethod( const char* fname )
	{
		using type = std::conditional_t<std::is_pointer_v<F>, F, std::add_pointer_t<F>>;
		CUtlString fullname = FunctionString<type>( fname ) + " const";

#ifdef __DEBUG_DevMsg__
		DevMsg( "%s::constmethod %s\n", name.Get(), fullname.Get() );
#endif
		int _id = engine->RegisterInterfaceMethod( name.Get(), fullname.Get() );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Interface::constmethod (%s::%s) RegisterInterfaceMethod failed %d", name.Get(), fullname.Get(), _id );
		return *this;
	}
};

template <typename T, ASClassType class_type = class_ref>
class Class
{
	asIScriptEngine*	engine;
	CUtlString			name;

	// TODO: flag value/reference
	void registerSelf( int extraFlags = 0 )
	{
		int flags = 0;
		int size = 0;
		switch ( class_type )
		{
			case class_pod:
				flags = asOBJ_APP_CLASS | asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<T>();
				size = sizeof( T );
				break;
			case class_class:
				flags = asOBJ_VALUE | asOBJ_APP_CLASS_CDAK;
				size = sizeof( T );
				break;
			case class_singleref:
				flags = asOBJ_REF | asOBJ_NOHANDLE;
				size = 0;
				break;
			case class_pod_allints:
				flags = asOBJ_APP_CLASS | asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_ALLINTS | asGetTypeTraits<T>();
				size = sizeof( T );
				break;
			case class_pod_allfloats:
				flags = asOBJ_APP_CLASS | asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_ALLFLOATS | asGetTypeTraits<T>();
				size = sizeof( T );
				break;
			case class_nocount:
				flags = asOBJ_REF | asOBJ_NOCOUNT;
				size = 0;
				break;
			case class_ref:
			default:
				flags = asOBJ_REF;
				size = 0;
				break;
		}

		int id = engine->RegisterObjectType( name.Get(), size, flags | extraFlags );
		Assert( id >= 0 );
		if ( id < 0 )
			Error( "ASBind::Class (%s) RegisterObjectType failed %d", name.Get(), id );

#ifdef __DEBUG_DevMsg__
		DevMsg( "ASBind::Class registered new class %s\n", name.Get() );
#endif
	}

public:
	// constructors
	Class( asIScriptEngine* engine, int extraFlags = 0 ) : engine( engine )
	{
		name = TypeString<T>();
		registerSelf( extraFlags );
	}

	Class( asIScriptEngine* engine, const char* name, int extraFlags = 0 )
		: engine( engine ), name( name )
	{
		registerSelf( extraFlags );
	}

	// not "public" constructor that doesnt register the type
	// but creates a proxy for it
	Class( asIScriptEngine* engine, const char* name, bool )
		: engine( engine ), name( name )
	{}

	// BIND METHOD

	// input actual method function of class
	template <typename F>
	Class& method( F f, const char* fname )
	{
		static_assert( std::is_function_v<std::remove_pointer_t<F>> || std::is_member_function_pointer_v<F> );

		asECallConvTypes conv;
		CUtlString fullname;
		asSFuncPtr func;
		if constexpr ( std::is_member_function_pointer_v<F> )
		{
			conv	 = asCALL_THISCALL;
			fullname = MethodString<F>( fname );
			func	 = asSMethodPtr<sizeof( f )>::Convert( f );
		}
		else
		{
			using t = typename StripThisProxy<F>::Args;
			static_assert( std::tuple_size_v<t> >= 1 && ( (std::tuple_size_v<t> == 1 && std::is_pointer_v<std::tuple_element_t<0, t>> && std::is_base_of_v<std::remove_pointer_t<std::tuple_element_t<0, t>>, T>) ||
					( ( std::is_pointer_v<std::tuple_element_t<0, t>> || std::is_pointer_v<std::tuple_element_t<std::tuple_size_v<t> - 1, t>> ) &&
					( std::is_base_of_v<std::remove_pointer_t<std::tuple_element_t<0, t>>, T> ^ std::is_base_of_v<std::remove_pointer_t<std::tuple_element_t<std::tuple_size_v<t> - 1, t>>, T> ) ) ) );
			if constexpr ( std::is_base_of_v<std::remove_pointer_t<std::tuple_element_t<0, t>>, T> )
			{
				fullname = FunctionString<typename StripThisProxy<F>::func_of>( fname );
				conv	 = asCALL_CDECL_OBJFIRST;
			}
			else
			{
				fullname = FunctionString<typename StripThisProxy<F>::func_ol>( fname );
				conv	 = asCALL_CDECL_OBJLAST;
			}
			func = asFunctionPtr( f );
		}

#ifdef __DEBUG_DevMsg__
		DevMsg( "%s::method %s\n", name.Get(), fullname.Get() );
#endif

		int _id = engine->RegisterObjectMethod( name.Get(), fullname.Get(), func, conv );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Class::method (%s::%s) RegisterObjectMethod failed %d", name.Get(), fullname.Get(), _id );
		return *this;
	}

	// explicit prototype + name
	template <typename F>
	Class& method2( F f, const char* proto )
	{
		static_assert( std::is_function_v<std::remove_pointer_t<F>> || std::is_member_function_pointer_v<F> );

		asECallConvTypes conv;
		asSFuncPtr func;
		if constexpr ( std::is_member_function_pointer_v<F> )
		{
			conv = asCALL_THISCALL;
			func = asSMethodPtr<sizeof( f )>::Convert( f );
		}
		else
		{
			using t = typename StripThisProxy<F>::Args;
			static_assert( std::tuple_size_v<t> >= 1 && ( (std::tuple_size_v<t> == 1 && std::is_pointer_v<std::tuple_element_t<0, t>> && std::is_base_of_v<std::remove_pointer_t<std::tuple_element_t<0, t>>, T>) ||
					( ( std::is_pointer_v<std::tuple_element_t<0, t>> || std::is_pointer_v<std::tuple_element_t<std::tuple_size_v<t> - 1, t>> ) &&
					( std::is_base_of_v<std::remove_pointer_t<std::tuple_element_t<0, t>>, T> ^ std::is_base_of_v<std::remove_pointer_t<std::tuple_element_t<std::tuple_size_v<t> - 1, t>>, T> ) ) ) );
			if constexpr ( std::is_base_of_v<std::remove_pointer_t<std::tuple_element_t<0, t>>, T> )
				conv = asCALL_CDECL_OBJFIRST;
			else
				conv = asCALL_CDECL_OBJLAST;
			func = asFunctionPtr( f );
		}

#ifdef __DEBUG_DevMsg__
		DevMsg( "%s::method %s\n", name.Get(), proto );
#endif

		int _id = engine->RegisterObjectMethod( name.Get(), proto, func, conv );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Class::method (%s::%s) RegisterObjectMethod failed %d", name.Get(), proto, _id );
		return *this;
	}

	// const methods get the 'const' qualifier automatically, but this is here in case
	// you want explicit const method from non-const method
	// input actual method function of class TODO: migrate method + constmethod
	template <typename F>
	Class& constmethod( F f, const char* fname )
	{
		static_assert( std::is_function_v<std::remove_pointer_t<F>> || std::is_member_function_pointer_v<F> );

		asECallConvTypes conv;
		CUtlString fullname;
		asSFuncPtr func;
		if constexpr ( std::is_member_function_pointer_v<F> )
		{
			conv	 = asCALL_THISCALL;
			fullname = MethodString<F>( fname ) + " const";
			func	 = asSMethodPtr<sizeof( f )>::Convert( f );
		}
		else
		{
			using t = typename StripThisProxy<F>::Args;
			static_assert( std::tuple_size_v<t> >= 1 && ( (std::tuple_size_v<t> == 1 && std::is_pointer_v<std::tuple_element_t<0, t>> && std::is_base_of_v<std::remove_pointer_t<std::tuple_element_t<0, t>>, T>) ||
					( ( std::is_pointer_v<std::tuple_element_t<0, t>> || std::is_pointer_v<std::tuple_element_t<std::tuple_size_v<t> - 1, t>> ) &&
					( std::is_base_of_v<std::remove_pointer_t<std::tuple_element_t<0, t>>, T> ^ std::is_base_of_v<std::remove_pointer_t<std::tuple_element_t<std::tuple_size_v<t> - 1, t>>, T> ) ) ) );
			if constexpr ( std::is_base_of_v<std::remove_pointer_t<std::tuple_element_t<0, t>>, T> )
			{
				fullname = FunctionString<typename StripThisProxy<F>::func_of>( fname ) + " const";
				conv = asCALL_CDECL_OBJFIRST;
			}
			else
			{
				fullname = FunctionString<typename StripThisProxy<F>::func_ol>( fname ) + " const";
				conv = asCALL_CDECL_OBJLAST;
			}
			func = asFunctionPtr( f );
		}

#ifdef __DEBUG_DevMsg__
		DevMsg( "%s::constmethod %s\n", name.Get(), fullname.Get() );
#endif

		int _id = engine->RegisterObjectMethod( name.Get(), fullname.Get(), func, conv );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Class::constmethod (%s::%s) RegisterObjectMethod failed %d", name.Get(), fullname.Get(), _id );
		return *this;
	}

	// const methods get the 'const' qualifier automatically, but this is here in case
	// you want explicit const method from non-const method
	// input actual method function of class TODO: migrate method + constmethod
	template <typename S, typename R, typename... Args>
	std::enable_if_t<std::is_base_of_v<S, T>, Class&> constmethod( R( S::* f )( Args... ) const, const char* fname )
	{
		CUtlString constname = MethodString<R( T::* )( Args... ) const>( fname );
#ifdef __DEBUG_DevMsg__
		DevMsg( "%s::constmethod %s\n", name.Get(), constname.Get() );
#endif

		int _id = engine->RegisterObjectMethod( name.Get(), constname.Get(), asSMethodPtr<sizeof( f )>::Convert( f ), asCALL_THISCALL );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Class::constmethod (%s::%s) RegisterObjectMethod failed %d", name.Get(), constname.Get(), _id );
		return *this;
	}

	template <typename S1, typename S2, typename A1, typename A2>
	std::enable_if_t<std::is_base_of_v<S1, T> && std::is_base_of_v<S2, T> && std::is_same_v<std::decay_t<A1>, std::decay_t<A2>>, Class&> methodproperty( A1( S1::* getter )() const, void( S2::* setter )( A2 ), const char* fname )
	{
		CUtlString get_name = MethodString<A1( S1::* )()>( ( CUtlString( "get_" ) + fname ).Get() ) + " const property";
		CUtlString set_name = MethodString<void( S2::* )( A2 )>( ( CUtlString( "set_" ) + fname ).Get() ) + " property";
#ifdef __DEBUG_DevMsg__
		DevMsg( "%s::methodproperty %s %s\n", name.Get(), get_name.Get(), set_name.Get() );
#endif

		int _id = engine->RegisterObjectMethod( name.Get(), get_name.Get(), asSMethodPtr<sizeof( getter )>::Convert( getter ), asCALL_THISCALL );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Class::methodproperty (%s::%s) RegisterObjectMethod failed %d", name.Get(), get_name.Get(), _id );

		_id = engine->RegisterObjectMethod( name.Get(), set_name.Get(), asSMethodPtr<sizeof( setter )>::Convert( setter ), asCALL_THISCALL );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Class::methodproperty (%s::%s) RegisterObjectMethod failed %d", name.Get(), set_name.Get(), _id );
		return *this;
	}

	template <typename S1, typename S2, typename A1, typename A2>
	std::enable_if_t<std::is_base_of_v<S1, T> && std::is_base_of_v<S2, T> && std::is_same_v<std::decay_t<A1>, std::decay_t<A2>>, Class&> methodproperty( A1( S1::* getter )(), void( S2::* setter )( A2 ), const char* fname )
	{
		CUtlString get_name = MethodString<A1( S1::* )()>( ( CUtlString( "get_" ) + fname ).Get() ) + " const property";
		CUtlString set_name = MethodString<void( S2::* )( A2 )>( ( CUtlString( "set_" ) + fname ).Get() ) + " property";
#ifdef __DEBUG_DevMsg__
		DevMsg( "%s::methodproperty %s %s\n", name.Get(), get_name.Get(), set_name.Get() );
#endif

		int _id = engine->RegisterObjectMethod( name.Get(), get_name.Get(), asSMethodPtr<sizeof( getter )>::Convert( getter ), asCALL_THISCALL );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Class::methodproperty (%s::%s) RegisterObjectMethod failed %d", name.Get(), get_name.Get(), _id );

		_id = engine->RegisterObjectMethod( name.Get(), set_name.Get(), asSMethodPtr<sizeof( setter )>::Convert( setter ), asCALL_THISCALL );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Class::methodproperty (%s::%s) RegisterObjectMethod failed %d", name.Get(), set_name.Get(), _id );
		return *this;
	}

	template <typename S1, typename S2, typename A1, typename A2>
	std::enable_if_t<std::is_base_of_v<S1, T> && std::is_base_of_v<S2, T> && std::is_same_v<std::decay_t<A1>, std::decay_t<A2>>, Class&> arrayindex( A1( S1::* getter )( int ), void( S2::* setter )( int, A2 ) )
	{
		CUtlString get_name = MethodString<A1( S1::* )( int )>( "get_opIndex" ) + " const property";
		CUtlString set_name = MethodString<void( S2::* )( int, A2 )>( "set_opIndex" ) + " property";
#ifdef __DEBUG_DevMsg__
		DevMsg( "%s::methodproperty %s %s\n", name.Get(), get_name.Get(), set_name.Get() );
#endif

		int _id = engine->RegisterObjectMethod( name.Get(), get_name.Get(), asSMethodPtr<sizeof( getter )>::Convert( getter ), asCALL_THISCALL );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Class::methodproperty (%s::%s) RegisterObjectMethod failed %d", name.Get(), get_name.Get(), _id );

		_id = engine->RegisterObjectMethod( name.Get(), set_name.Get(), asSMethodPtr<sizeof( setter )>::Convert( setter ), asCALL_THISCALL );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Class::methodproperty (%s::%s) RegisterObjectMethod failed %d", name.Get(), set_name.Get(), _id );
		return *this;
	}

	// const methods get the 'const' qualifier automatically, but this is here in case
	// you want explicit const method from non-const method
	// input actual method function of class TODO: migrate method + constmethod
	template <typename S, typename R, typename... Args>
	std::enable_if_t<std::is_base_of_v<S, T>, Class&> constmethodproperty( R( S::* f )( Args... ) const, const char* fname )
	{
		static_assert( sizeof...( Args ) == 0 || sizeof...( Args ) == 1 );
		CUtlString constname = MethodString<R( T::* )( Args... ) const>( ( CUtlString( "get_" ) + fname ).Get() ) + " property";
#ifdef __DEBUG_DevMsg__
		DevMsg( "%s::constmethodproperty %s\n", name.Get(), constname.Get() );
#endif

		int _id = engine->RegisterObjectMethod( name.Get(), constname.Get(), asSMethodPtr<sizeof( f )>::Convert( f ), asCALL_THISCALL );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Class::constmethodproperty (%s::%s) RegisterObjectMethod failed %d", name.Get(), constname.Get(), _id );
		return *this;
	}

	template <typename S, typename R, typename... Args>
	std::enable_if_t<std::is_base_of_v<S, T>, Class&> constmethodproperty( R( S::* f )( Args... ), const char* fname )
	{
		static_assert( sizeof...( Args ) == 0 || sizeof...( Args ) == 1 );
		CUtlString constname = MethodString<R( T::* )( Args... ) const>( ( CUtlString( "get_" ) + fname ).Get() ) + " property";
#ifdef __DEBUG_DevMsg__
		DevMsg( "%s::constmethodproperty %s\n", name.Get(), constname.Get() );
#endif

		int _id = engine->RegisterObjectMethod( name.Get(), constname.Get(), asSMethodPtr<sizeof( f )>::Convert( f ), asCALL_THISCALL );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Class::constmethodproperty (%s::%s) RegisterObjectMethod failed %d", name.Get(), constname.Get(), _id );
		return *this;
	}

	// input global function with* this either first or last parameter (obj_first)
	template <typename F>
	Class& method( F f, const char* fname, bool obj_first )
	{
		CUtlString funcname = obj_first ?
							   FunctionString<typename StripThisProxy<F>::func_of>( fname ) :
							   FunctionString<typename StripThisProxy<F>::func_ol>( fname );

#ifdef __DEBUG_DevMsg__
		DevMsg( "%s::method %s\n", name.Get(), funcname.Get() );
#endif

		int _id = engine->RegisterObjectMethod( name.Get(), funcname.Get(), asFUNCTION( f ), obj_first ? asCALL_CDECL_OBJFIRST : asCALL_CDECL_OBJLAST );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Class::method (%s::%s) RegisterObjectMethod failed %d", name.Get(), funcname.Get(), _id );
		return *this;
	}

	// input global function with* this either first or last parameter (obj_first)
	template <typename F>
	Class& method2( F f, const char* fname, bool obj_first )
	{
#ifdef __DEBUG_DevMsg__
		DevMsg( "%s::method %s\n", name.Get(), fname );
#endif

		int _id = engine->RegisterObjectMethod( name.Get(), fname, asFUNCTION( f ), obj_first ? asCALL_CDECL_OBJFIRST : asCALL_CDECL_OBJLAST );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Class::method (%s::%s) RegisterObjectMethod failed %d", name.Get(), fname, _id );
		return *this;
	}

	// input global function with* this either first or last parameter (obj_first)
	template <typename F>
	Class& constmethod( F f, const char* fname, bool obj_first )
	{
		CUtlString constname = ( obj_first ?
								  FunctionString<typename StripThisProxy<F>::func_of>( fname ) :
								  FunctionString<typename StripThisProxy<F>::func_ol>( fname ) ) + " const";

#ifdef __DEBUG_DevMsg__
		DevMsg( "%s::constmethod %s\n", name.Get(), constname.Get() );
#endif

		int _id = engine->RegisterObjectMethod( name.Get(), constname.Get(), asFUNCTION( f ), obj_first ? asCALL_CDECL_OBJFIRST : asCALL_CDECL_OBJLAST );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Class::constmethod (%s::%s) RegisterObjectMethod failed %d", name.Get(), constname.Get(), _id );
		return *this;
	}

	template <typename F1, typename F2>
	Class& methodproperty( F1 getter, F2 setter, const char* fname )
	{
		static_assert( std::is_function_v<std::remove_pointer_t<F1>> && std::is_function_v<std::remove_pointer_t<F2>> );

		using t1 = typename StripThisProxy<F1>::Args;
		static_assert( std::tuple_size_v<t1> == 1 && std::is_pointer_v<std::tuple_element_t<0, t1>> && std::is_base_of_v<std::remove_pointer_t<std::tuple_element_t<0, t1>>, T> );

		using t2 = typename StripThisProxy<F2>::Args;
		static_assert(
					( std::tuple_size_v<t2> == 2 && ( std::is_pointer_v<std::tuple_element_t<0, t2>> || std::is_pointer_v<std::tuple_element_t<std::tuple_size_v<t2> - 1, t2>> ) &&
					( std::is_base_of_v<std::remove_pointer_t<std::tuple_element_t<0, t2>>, T> ^ std::is_base_of_v<std::remove_pointer_t<std::tuple_element_t<std::tuple_size_v<t2> - 1, t2>>, T> ) ) );

		CUtlString get_name;
		asECallConvTypes get_conv;
		CUtlString set_name;
		asECallConvTypes set_conv;

		if constexpr ( std::is_base_of_v<std::remove_pointer_t<std::tuple_element_t<0, t1>>, T> )
		{
			get_name = FunctionString<typename StripThisProxy<F1>::func_of>( ( CUtlString( "get_" ) + fname ).Get() ) + " const property";
			get_conv = asCALL_CDECL_OBJFIRST;
		}
		else
		{
			get_name = FunctionString<typename StripThisProxy<F1>::func_ol>( ( CUtlString( "get_" ) + fname ).Get() ) + " const property";
			get_conv = asCALL_CDECL_OBJLAST;
		}

		if constexpr ( std::is_base_of_v<std::remove_pointer_t<std::tuple_element_t<0, t2>>, T> )
		{
			set_name = FunctionString<typename StripThisProxy<F2>::func_of>( ( CUtlString( "set_" ) + fname ).Get() ) + " property";
			set_conv = asCALL_CDECL_OBJFIRST;
		}
		else
		{
			set_name = FunctionString<typename StripThisProxy<F2>::func_ol>( ( CUtlString( "set_" ) + fname ).Get() ) + " property";
			set_conv = asCALL_CDECL_OBJLAST;
		}

#ifdef __DEBUG_DevMsg__
		DevMsg( "%s::methodproperty %s %s\n", name.Get(), get_name.Get(), set_name.Get() );
#endif

		int _id = engine->RegisterObjectMethod( name.Get(), get_name.Get(), asFUNCTION( getter ), get_conv );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Class::methodproperty (%s::%s) RegisterObjectMethod failed %d", name.Get(), get_name.Get(), _id );

		_id = engine->RegisterObjectMethod( name.Get(), set_name.Get(), asFUNCTION( setter ), set_conv );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Class::methodproperty (%s::%s) RegisterObjectMethod failed %d", name.Get(), set_name.Get(), _id );
		return *this;
	}

	// input global function with* this either first or last parameter (obj_first)
	template <typename F>
	Class& constmethodproperty( F f, const char* fname )
	{
		static_assert( std::is_function_v<std::remove_pointer_t<F>> );

		using t = typename StripThisProxy<F>::Args;
		static_assert( (std::tuple_size_v<t> == 1 && std::is_pointer_v<std::tuple_element_t<0, t>> && std::is_base_of_v<std::remove_pointer_t<std::tuple_element_t<0, t>>, T>) ||
					( std::tuple_size_v<t> == 2 && ( std::is_pointer_v<std::tuple_element_t<0, t>> || std::is_pointer_v<std::tuple_element_t<std::tuple_size_v<t> - 1, t>> ) &&
					( std::is_base_of_v<std::remove_pointer_t<std::tuple_element_t<0, t>>, T> ^ std::is_base_of_v<std::remove_pointer_t<std::tuple_element_t<std::tuple_size_v<t> - 1, t>>, T> ) ) );

		asECallConvTypes conv;
		CUtlString constname;
		if constexpr ( std::is_base_of_v<std::remove_pointer_t<std::tuple_element_t<0, t>>, T> )
		{
			constname = FunctionString<typename StripThisProxy<F>::func_of>( ( CUtlString( "get_" ) + fname ).Get() ) + " const property";
			conv = asCALL_CDECL_OBJFIRST;
		}
		else
		{
			constname = FunctionString<typename StripThisProxy<F>::func_ol>( ( CUtlString( "get_" ) + fname ).Get() ) + " const property";
			conv = asCALL_CDECL_OBJLAST;
		}

#ifdef __DEBUG_DevMsg__
		DevMsg( "%s::constmethodproperty %s\n", name.Get(), constname.Get() );
#endif

		int _id = engine->RegisterObjectMethod( name.Get(), constname.Get(), asFUNCTION( f ), conv );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Class::constmethodproperty (%s::%s) RegisterObjectMethod failed %d", name.Get(), constname.Get(), _id );
		return *this;
	}

	template <typename F>
	Class& constmethodproperty( F f, const char* fname, bool obj_first )
	{
		CUtlString constname = ( obj_first ?
								  FunctionString<typename StripThisProxy<F>::func_of>( ( CUtlString( "get_" ) + fname ).Get() ) :
								  FunctionString<typename StripThisProxy<F>::func_ol>( ( CUtlString( "get_" ) + fname ).Get() ) ) + " const property";

#ifdef __DEBUG_DevMsg__
		DevMsg( "%s::constmethodproperty %s\n", name.Get(), constname.Get() );
#endif

		int _id = engine->RegisterObjectMethod( name.Get(), constname.Get(), asFUNCTION( f ), obj_first ? asCALL_CDECL_OBJFIRST : asCALL_CDECL_OBJLAST );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Class::constmethodproperty (%s::%s) RegisterObjectMethod failed %d", name.Get(), constname.Get(), _id );
		return *this;
	}

	// BIND MEMBER
	template <typename V, typename..., typename T2 = T>
	std::enable_if_t<std::is_class_v<T2>, Class&> member( V T2::*v, const char* mname )
	{
		CUtlString fullname = TypeString<V>( mname );

#ifdef __DEBUG_DevMsg__
		DevMsg( "%s::member %s\n", name.Get(), fullname.Get() );
#endif

		// int _id = engine->RegisterObjectProperty( name.Get(), fullname.Get(), offsetof( T, *pv ) );
		int _id = engine->RegisterObjectProperty( name.Get(), fullname.Get(), reinterpret_cast<size_t>( &( static_cast<T*>( 0 )->*v ) ) ); // damn gcc
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Class::member (%s::%s) RegisterObjectProperty failed %d", name.Get(), fullname.Get(), _id );
		return *this;
	}

	// BIND MEMBER CONST
	template <typename V, typename..., typename T2 = T>
	std::enable_if_t<std::is_class_v<T2>, Class&> constmember( V T2::*v, const char* mname )
	{
		CUtlString fullname = TypeString<const V>( mname );

#ifdef __DEBUG_DevMsg__
		DevMsg( "%s::member %s\n", name.Get(), fullname.Get() );
#endif

		// int _id = engine->RegisterObjectProperty( name.Get(), fullname.Get(), offsetof( T, *pv ) );
		int _id = engine->RegisterObjectProperty( name.Get(), fullname.Get(), reinterpret_cast<size_t>( &( static_cast<T*>( 0 )->*v ) ) ); // damn gcc
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Class::constmember (%s::%s) RegisterObjectProperty failed %d", name.Get(), fullname.Get(), _id );
		return *this;
	}

	// BEHAVIOURS

	// REFERENCE

	// input methods of the class
	template <typename..., typename T2 = T>
	std::enable_if_t<std::is_class_v<T2>, Class&> refs( void ( T2::*addref )(), void ( T2::*release )() )
	{
		int _id = engine->RegisterObjectBehaviour( name.Get(), asBEHAVE_ADDREF, "void f()", asSMethodPtr<sizeof( addref )>::Convert( addref ), asCALL_THISCALL );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Class::refs (%s) RegisterObjectBehaviour failed %d", name.Get(), _id );

		_id = engine->RegisterObjectBehaviour( name.Get(), asBEHAVE_RELEASE, "void f()", asSMethodPtr<sizeof( release )>::Convert( release ), asCALL_THISCALL );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Class::refs (%s) RegisterObjectBehaviour failed %d", name.Get(), _id );
		return *this;
	}

	// input global functions
	template <typename..., typename T2 = T>
	std::enable_if_t<std::is_class_v<T2>, Class&> refs( void ( *addref )( T* ), void ( *release )( T* ) )
	{
		int _id = engine->RegisterObjectBehaviour( name.Get(), asBEHAVE_ADDREF, "void f()", asFUNCTION( addref ), asCALL_CDECL_OBJLAST );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Class::refs (%s) RegisterObjectBehaviour failed %d", name.Get(), _id );

		_id = engine->RegisterObjectBehaviour( name.Get(), asBEHAVE_RELEASE, "void f()", asFUNCTION( release ), asCALL_CDECL_OBJLAST );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Class::refs (%s) RegisterObjectBehaviour failed %d", name.Get(), _id );
		return *this;
	}

	// FACTORY
	// input global function that may or may not take parameters
	template <typename F>
	Class& factory( F f )
	{
		// FIXME: config CDECL/STDCALL!
		int _id = engine->RegisterObjectBehaviour( name.Get(), asBEHAVE_FACTORY, FunctionString<F>( "f" ).Get(), asFUNCTION( f ), asCALL_CDECL );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Class::factory (%s) RegisterObjectBehaviour failed %d", name.Get(), _id );
		return *this;
	}

	// CONSTRUCTOR
	// input constructor type as <void(parameters)> in template arguments
	template <typename F>
	Class& constructor()
	{
		int _id = engine->RegisterObjectBehaviour( name.Get(), asBEHAVE_CONSTRUCT, FunctionString<F>( "f" ).Get(), asFunctionPtr( CallCtorProxy<T, F>::ctor() ), asCALL_CDECL_OBJFIRST );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Class::constructor (%s) RegisterObjectBehaviour failed %d", name.Get(), _id );
		return *this;
	}

	// input global function (see helper)
	template <typename F>
	Class& constructor( F f )
	{
		static_assert( std::is_function_v<std::remove_pointer_t<F>> );

		CUtlString funcname;
		asECallConvTypes conv;
		using t = typename StripThisProxy<F>::Args;
		static_assert( std::tuple_size_v<t> >= 1 && ( (std::tuple_size_v<t> == 1 && std::is_pointer_v<std::tuple_element_t<0, t>> && std::is_base_of_v<std::remove_pointer_t<std::tuple_element_t<0, t>>, T>) ||
				( ( std::is_pointer_v<std::tuple_element_t<0, t>> || std::is_pointer_v<std::tuple_element_t<std::tuple_size_v<t> - 1, t>> ) &&
				( std::is_base_of_v<std::remove_pointer_t<std::tuple_element_t<0, t>>, T> ^ std::is_base_of_v<std::remove_pointer_t<std::tuple_element_t<std::tuple_size_v<t> - 1, t>>, T> ) ) ) );
		if constexpr ( std::is_base_of_v<std::remove_pointer_t<std::tuple_element_t<0, t>>, T> )
		{
			funcname = FunctionString<typename StripThisProxy<F>::func_of>( "f" );
			conv = asCALL_CDECL_OBJFIRST;
		}
		else
		{
			funcname = FunctionString<typename StripThisProxy<F>::func_ol>( "f" );
			conv = asCALL_CDECL_OBJLAST;
		}

#ifdef __DEBUG_DevMsg__
		DevMsg( "%s::constructor %s\n", name.Get(), funcname.Get() );
#endif

		int _id = engine->RegisterObjectBehaviour( name.Get(), asBEHAVE_CONSTRUCT, funcname.Get(), asFUNCTION( f ), conv );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Class::constructor (%s) RegisterObjectBehaviour failed %d", name.Get(), _id );
		return *this;
	}

	template <typename F>
	Class& constructor( F f, bool obj_first )
	{
		static_assert( std::is_function_v<std::remove_pointer_t<F>> );

		CUtlString funcname = obj_first ?
								FunctionString<typename StripThisProxy<F>::func_of>( "f" ) :
								FunctionString<typename StripThisProxy<F>::func_ol>( "f" );

#ifdef __DEBUG_DevMsg__
		DevMsg( "%s::constructor %s\n", name.Get(), funcname.Get() );
#endif

		int _id = engine->RegisterObjectBehaviour( name.Get(), asBEHAVE_CONSTRUCT, funcname.Get(), asFUNCTION( f ), obj_first ? asCALL_CDECL_OBJFIRST : asCALL_CDECL_OBJLAST );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Class::constructor (%s) RegisterObjectBehaviour failed %d", name.Get(), _id );
		return *this;
	}

	// DECONSTRUCTOR
	// automatic object destructor
	Class& destructor()
	{
		int _id = engine->RegisterObjectBehaviour( name.Get(), asBEHAVE_DESTRUCT, "void f()", asFUNCTION( Destruct<T> ), asCALL_CDECL_OBJLAST );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Class::destructor (%s) RegisterObjectBehaviour failed %d", name.Get(), _id );
		return *this;
	}

	// input global function (see helper for this)
	Class& destructor( void ( *f )( T* ) )
	{
		int _id = engine->RegisterObjectBehaviour( name.Get(), asBEHAVE_DESTRUCT, "void f()", asFUNCTION( f ), asCALL_CDECL_OBJLAST );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Class::destructor (%s) RegisterObjectBehaviour failed %d", name.Get(), _id );
		return *this;
	}

	// CAST

	// input method
	template <typename F>
	Class& cast( F f, bool implicit_cast = false )
	{
		static_assert( std::is_function_v<std::remove_pointer_t<F>> || std::is_member_function_pointer_v<F> );

		asECallConvTypes conv;
		CUtlString fullname;
		asSFuncPtr func;
		if constexpr ( std::is_member_function_pointer_v<F> )
		{
			conv	 = asCALL_THISCALL;
			fullname = MethodString<F>( implicit_cast ? "opImplConv" : "opConv" );
			func	 = asSMethodPtr<sizeof( f )>::Convert( f );
		}
		else
		{
			using t = typename StripThisProxy<F>::Args;
			static_assert( std::tuple_size_v<t> >= 1 && ( (std::tuple_size_v<t> == 1 && std::is_pointer_v<std::tuple_element_t<0, t>> && std::is_base_of_v<std::remove_pointer_t<std::tuple_element_t<0, t>>, T>) ||
					( ( std::is_pointer_v<std::tuple_element_t<0, t>> || std::is_pointer_v<std::tuple_element_t<std::tuple_size_v<t> - 1, t>> ) &&
					( std::is_base_of_v<std::remove_pointer_t<std::tuple_element_t<0, t>>, T> ^ std::is_base_of_v<std::remove_pointer_t<std::tuple_element_t<std::tuple_size_v<t> - 1, t>>, T> ) ) ) );
			if constexpr ( std::is_base_of_v<std::remove_pointer_t<std::tuple_element_t<0, t>>, T> )
			{
				fullname = FunctionString<typename StripThisProxy<F>::func_of>( implicit_cast ? "opImplConv" : "opConv" );
				conv = asCALL_CDECL_OBJFIRST;
			}
			else
			{
				fullname = FunctionString<typename StripThisProxy<F>::func_ol>( implicit_cast ? "opImplConv" : "opConv" );
				conv = asCALL_CDECL_OBJLAST;
			}
			func = asFunctionPtr( f );
		}

		int _id = engine->RegisterObjectMethod( name.Get(), fullname.Get(), func, conv );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Class::cast (%s) RegisterObjectBehaviour failed %d", name.Get(), _id );
		return *this;
	}

	// input global function with* this either first or last parameter (obj_first)
	template <typename F>
	Class& cast( F f, bool implicit_cast, bool obj_first )
	{
		CUtlString funcname = obj_first ?
							   FunctionString<typename StripThisProxy<F>::func_of>( implicit_cast ? "opImplConv" : "opConv" ) :
							   FunctionString<typename StripThisProxy<F>::func_ol>( implicit_cast ? "opImplConv" : "opConv" );

#ifdef __DEBUG_DevMsg__
		DevMsg( "%s::cast %s\n", name.Get(), funcname.Get() );
#endif

		int _id = engine->RegisterObjectMethod( name.Get(), funcname.Get(), asFUNCTION( f ), obj_first ? asCALL_CDECL_OBJFIRST : asCALL_CDECL_OBJLAST );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Class::cast (%s::%s) RegisterObjectMethod failed %d", name.Get(), funcname.Get(), _id );
		return *this;
	}

	template <typename F>
	Class& refcast( F f, bool implicit_cast = false )
	{
		static_assert( std::is_function_v<std::remove_pointer_t<F>> || std::is_member_function_pointer_v<F> );

		asECallConvTypes conv;
		CUtlString fullname;
		asSFuncPtr func;
		if constexpr ( std::is_member_function_pointer_v<F> )
		{
			conv	 = asCALL_THISCALL;
			fullname = MethodString<F>( implicit_cast ? "opImplCast" : "opCast" );
			func	 = asSMethodPtr<sizeof( f )>::Convert( f );
		}
		else
		{
			using t = typename StripThisProxy<F>::Args;
			static_assert( std::tuple_size_v<t> >= 1 && ( (std::tuple_size_v<t> == 1 && std::is_pointer_v<std::tuple_element_t<0, t>> && std::is_base_of_v<std::remove_pointer_t<std::tuple_element_t<0, t>>, T>) ||
					( ( std::is_pointer_v<std::tuple_element_t<0, t>> || std::is_pointer_v<std::tuple_element_t<std::tuple_size_v<t> - 1, t>> ) &&
					( std::is_base_of_v<std::remove_pointer_t<std::tuple_element_t<0, t>>, T> ^ std::is_base_of_v<std::remove_pointer_t<std::tuple_element_t<std::tuple_size_v<t> - 1, t>>, T> ) ) ) );
			if constexpr ( std::is_base_of_v<std::remove_pointer_t<std::tuple_element_t<0, t>>, T> )
			{
				fullname = FunctionString<typename StripThisProxy<F>::func_of>( implicit_cast ? "opImplCast" : "opCast" );
				conv = asCALL_CDECL_OBJFIRST;
			}
			else
			{
				fullname = FunctionString<typename StripThisProxy<F>::func_ol>( implicit_cast ? "opImplCast" : "opCast" );
				conv = asCALL_CDECL_OBJLAST;
			}
			func = asFunctionPtr( f );
		}

		int _id = engine->RegisterObjectMethod( name.Get(), fullname.Get(), func, conv );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Class::cast (%s) RegisterObjectBehaviour failed %d", name.Get(), _id );
		return *this;
	}

	// input global function with* this either first or last parameter (obj_first)
	template <typename F>
	Class& refcast( F f, bool implicit_cast, bool obj_first )
	{
		CUtlString funcname = obj_first ?
								FunctionString<typename StripThisProxy<F>::func_of>( implicit_cast ? "opImplCast" : "opCast" ) :
								FunctionString<typename StripThisProxy<F>::func_ol>( implicit_cast ? "opImplCast" : "opCast" );

#ifdef __DEBUG_DevMsg__
		DevMsg( "%s::cast %s\n", name.Get(), funcname.Get() );
#endif

		// select the cast type
		int _id = engine->RegisterObjectMethod( name.Get(), funcname.Get(), asFUNCTION( f ), obj_first ? asCALL_CDECL_OBJFIRST : asCALL_CDECL_OBJLAST );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Class::cast (%s::%s) RegisterObjectMethod failed %d", name.Get(), funcname.Get(), _id );
		return *this;
	}

	// input method
	template <typename F>
	Class& assign( F f )
	{
		CUtlString funcname = MethodString<F>( "opAssign" );
		int _id = engine->RegisterObjectMethod( name.Get(), funcname.Get(), asSMethodPtr<sizeof( f )>::Convert( f ), asCALL_THISCALL );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Class::cast (%s) RegisterObjectBehaviour failed %d", name.Get(), _id );
		return *this;
	}

	// input method
	template <typename F>
	Class& equals( F f )
	{
		CUtlString funcname = MethodString<F>( "opEquals" );
		int _id = engine->RegisterObjectMethod( name.Get(), funcname.Get(), asSMethodPtr<sizeof( f )>::Convert( f ), asCALL_THISCALL );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Class::cast (%s) RegisterObjectBehaviour failed %d", name.Get(), _id );
		return *this;
	}

	// input method
	template <typename F>
	Class& add( F f, bool right )
	{
		CUtlString funcname = MethodString<F>( right ? "opAdd_r" : "opAdd" );
		int _id = engine->RegisterObjectMethod( name.Get(), funcname.Get(), asSMethodPtr<sizeof( f )>::Convert( f ), asCALL_THISCALL );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Class::cast (%s) RegisterObjectBehaviour failed %d", name.Get(), _id );
		return *this;
	}

};

template <typename T>
Class<T> CreateClass( asIScriptEngine* engine )
{
	return Class<T>( engine );
}

template <typename T>
Class<T> CreateClass( asIScriptEngine* engine, const char* name )
{
	return Class<T>( engine, name );
}

template <typename T>
Class<T> GetClass( asIScriptEngine* engine, const CUtlString& name = TypeString<T>() )
{
	int i, count;

	count = engine->GetObjectTypeCount();
	for( i = 0; i < count; i++ )
	{
		asITypeInfo* obj = engine->GetObjectTypeByIndex( i );
		if ( obj && name == CUtlString( obj->GetName() ) )
		{
#ifdef __DEBUG_DevMsg__
			DevMsg( "GetClass found class %s\n", name.Get() );
#endif
			return Class<T>( engine, name, obj->GetTypeId() );
		}
	}

#ifdef __DEBUG_DevMsg__
	DevMsg( "GetClass creating new class %s\n", name.Get() );
#endif

	return Class<T>( engine, name );
}

//=========================================================

// asbind_global.h
// bind global functions and variables

class Global
{
	asIScriptEngine* engine;

public:
	// constructors
	Global( asIScriptEngine* engine ) : engine( engine ) {}

	// BIND FUNCTION
	// input global function with* this either first or last parameter (obj_first)
	template <typename F>
	Global& function( F f, const char* fname )
	{
		CUtlString funcname = FunctionString<F>( fname );
	#ifdef __DEBUG_DevMsg__
		DevMsg( "Global::function %s\n", funcname.Get() );
	#endif
		int _id = engine->RegisterGlobalFunction( funcname.Get(), asFUNCTION( f ), asCALL_CDECL );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Global::function (%s) RegisterGlobalFunction failed %d", funcname.Get(), _id );
		return *this;
	}

	template <typename F>
	Global& function2( F f, const char* fname )
	{
	#ifdef __DEBUG_DevMsg__
		DevMsg( "Global::function %s\n", fname );
	#endif
		int _id = engine->RegisterGlobalFunction( fname, asFUNCTION( f ), asCALL_CDECL );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Global::function (%s) RegisterGlobalFunction failed %d", fname, _id );
		return *this;
	}

	// BIND VARIABLE
	template <typename V>
	Global& var( V& v, const char* vname )
	{
		CUtlString varname = TypeString<V>( vname );
	#ifdef __DEBUG_DevMsg__
		DevMsg( "Global::var %s\n", varname.Get() );
	#endif
		int _id = engine->RegisterGlobalProperty( varname.Get(), (void*)&v );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Global::var (%s) RegisterGlobalProperty failed %d", varname.Get(), _id );
		return *this;
	}

	// BIND VARIABLE as pointer
	template <typename V>
	Global& var( V* v, const char* vname )
	{
		CUtlString varname = TypeString<V>( vname );
	#ifdef __DEBUG_DevMsg__
		DevMsg( "Global::var %s\n", varname.Get() );
	#endif
		int _id = engine->RegisterGlobalProperty( varname.Get(), (void*)v );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Global::var (%s) RegisterGlobalProperty failed %d", varname.Get(), _id );
		return *this;
	}

	// BIND constant
	template <typename V>
	Global& constvar( const V& v, const char* vname )
	{
		CUtlString varname = TypeString<const V>( vname );
	#ifdef __DEBUG_DevMsg__
		DevMsg( "Global::constvar %s\n", varname.Get() );
	#endif
		int _id = engine->RegisterGlobalProperty( varname.Get(), (void*)&v );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Global::constvar (%s) RegisterGlobalProperty failed %d", varname.Get(), _id );
		return *this;
	}

	// BIND constant as pointer
	template <typename V>
	Global& constvar( const V* v, const char* vname )
	{
		CUtlString varname = TypeString<const V>( vname );
	#ifdef __DEBUG_DevMsg__
		DevMsg( "Global::constvar %s\n", varname.Get() );
	#endif
		int _id = engine->RegisterGlobalProperty( varname.Get(), (void*)v );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Global::constvar (%s) RegisterGlobalProperty failed %d", varname.Get(), _id );
		return *this;
	}

	// BIND FUNCDEF
	template <typename F>
	Global& funcdef( F f, const char* fname )
	{
		CUtlString funcdefname = FuncdefString<F>( fname );
	#ifdef __DEBUG_DevMsg__
		DevMsg( "Global::funcdef %s\n", funcdefname.Get() );
	#endif
		int _id = engine->RegisterFuncdef( funcdefname.Get() );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Global::funcdef (%s) RegisterFuncdef failed %d", funcdefname.Get(), _id );
		return *this;
	}

	// CREATE CLASS
	template <typename T>
	Class<T> class_()
	{
		return Class<T>( engine );
	}

	template <typename T>
	Class<T> class_( const char* name )
	{
		return Class<T>( engine, name );
	}
};

//=========================================================

// asbind_enum.h
// bind enumerations

class Enum
{
	asIScriptEngine*	engine;
	CUtlString			name;

public:
	Enum( asIScriptEngine* _engine, const char* _name )
		: engine( _engine ), name( _name )
	{
		int id = engine->RegisterEnum( _name );
		Assert( id >= 0 );
		if ( id < 0 )
			Error( "ASBind::Enum RegisterEnum %s failed %d < 0", _name, id );
	}

	Enum& add( const char* key, int value )
	{
		int _id = engine->RegisterEnumValue( name.Get(), key, value );
		Assert( _id >= 0 );
		if ( _id < 0 )
			Error( "ASBind::Enum::add (%s %s) RegisterEnumValue failed %d", name.Get(), key, _id );
		return *this;
	}

	Enum& operator()( const char* key, int value )
	{
		return add( key, value );
	}

#ifdef NEARGYE_NAMEOF_HPP
	template <typename E>
	std::enable_if_t<std::is_enum_v<E>, Enum&> add( E value )
	{
		return add( nameof::nameof_enum( value ).data(), static_cast<int>( value ) );
	}

	template <typename E>
	std::enable_if_t<std::is_enum_v<E>> all()
	{
		constexpr auto values = nameof::detail::values_v<E>;
		for ( const auto v : values )
			add( nameof::nameof_enum( v ).data(), static_cast<int>( v ) );
	}

	template <typename E>
	std::enable_if_t<std::is_enum_v<E>> all_flags()
	{
		constexpr auto values = nameof::detail::flags_values_v<E>;
		for ( const auto v : values )
			add( nameof::nameof_enum2( v ).data(), static_cast<int>( v ) );
	}
#endif
};

//=========================================================

// asbind_utils.h
// miscellaneous helpers

// bindable reference objects can derive from this class and
// bind AddRef and Release as behaviour functions
struct RefWrapper
{
protected:
	int refcount;

public:
	RefWrapper() : refcount( 1 ) {}
	virtual ~RefWrapper() = default;

	void AddRef() { refcount++; }
	void Release()
	{
		refcount--;
		if ( refcount <= 0 )
			delete this;
	}
};

}