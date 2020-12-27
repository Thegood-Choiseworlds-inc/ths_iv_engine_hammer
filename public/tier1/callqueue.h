//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================

#ifndef CALLQUEUE_H
#define CALLQUEUE_H

#include "tier0/tslist.h"
#include "functors.h"

#if defined( _WIN32 )
#pragma once
#endif

//-----------------------------------------------------

template <typename QUEUE_TYPE = CTSQueue<CFunctor*>>
class CCallQueueT
{
public:
	CCallQueueT() : m_bNoQueue( false )
	{
#ifdef _DEBUG
		m_nCurSerialNumber = 0;
		m_nBreakSerialNumber = (unsigned)-1;
#endif
	}

	void DisableQueue( bool bDisable )
	{
		if ( m_bNoQueue == bDisable )
		{
			return;
		}
		if ( !m_bNoQueue )
			CallQueued();

		m_bNoQueue = bDisable;
	}

	bool IsDisabled() const
	{
		return m_bNoQueue;
	}

	int Count()
	{
		return m_queue.Count();
	}

	void CallQueued()
	{
		if ( !m_queue.Count() )
		{
			return;
		}

		m_queue.PushItem( nullptr );

		CFunctor *pFunctor;

		while ( m_queue.PopItem( &pFunctor ) && pFunctor != nullptr )
		{
#ifdef _DEBUG
			if ( pFunctor->m_nUserID == m_nBreakSerialNumber)
			{
				m_nBreakSerialNumber = (unsigned)-1;
			}
#endif
			(*pFunctor)();
			pFunctor->Release();
		}

	}

	void ParallelCallQueued( IThreadPool *pPool = NULL )
	{
		if ( !pPool )
		{
			pPool = g_pThreadPool;
		}
		int nNumThreads = 1;

		if ( pPool )
		{
			nNumThreads = MIN( pPool->NumThreads(), MAX( 1, Count() ) );
		}

		if ( nNumThreads < 2 )
		{
			CallQueued();
		}
		else
		{
			int *pDummy = NULL;
			ParallelProcess( "ParallelCallQueued", pPool, pDummy, nNumThreads, this, &CCallQueueT<>::ExecuteWrapper );
		}
	}

	void QueueFunctor( CFunctor *pFunctor )
	{
		Assert( pFunctor );
		QueueFunctorInternal( RetAddRef( pFunctor ) );
	}

	void Flush()
	{
		m_queue.PushItem( nullptr );

		CFunctor *pFunctor;

		while ( m_queue.PopItem( &pFunctor ) && pFunctor != nullptr )
		{
			pFunctor->Release();
		}
	}

	template <typename RetType, typename... FuncArgs, typename... Args, typename = decltype(std::declval<RetType(*)(FuncArgs...)>()(std::declval<Args>()...))>
	void QueueCall( RetType( *pfnProxied )( FuncArgs... ), Args&&... args )
	{
		QueueFunctorInternal( CreateFunctor( pfnProxied, std::forward<Args>( args )... ) );
	}

	template <class OBJECT_TYPE_PTR, typename FUNCTION_CLASS, typename FUNCTION_RETTYPE, typename... FuncArgs, typename... Args, typename = decltype(std::declval<FUNCTION_RETTYPE(*)(FuncArgs...)>()(std::declval<Args>()...))>
	auto QueueCall( OBJECT_TYPE_PTR pObject, FUNCTION_RETTYPE( FUNCTION_CLASS::* pfnProxied )( FuncArgs... ), Args&&... args ) -> std::enable_if_t<std::is_convertible_v<OBJECT_TYPE_PTR, FUNCTION_CLASS*>>
	{
		QueueFunctorInternal( CreateFunctor( pObject, pfnProxied, std::forward<Args>( args )... ) );
	}

	template <class OBJECT_TYPE_PTR, typename FUNCTION_CLASS, typename FUNCTION_RETTYPE, typename... FuncArgs, typename... Args, typename = decltype(std::declval<FUNCTION_RETTYPE(*)(FuncArgs...)>()(std::declval<Args>()...))>
	auto QueueCall( OBJECT_TYPE_PTR pObject, FUNCTION_RETTYPE( FUNCTION_CLASS::* pfnProxied )( FuncArgs... ) const, Args&&... args ) -> std::enable_if_t<std::is_convertible_v<OBJECT_TYPE_PTR, FUNCTION_CLASS*>>
	{
		QueueFunctorInternal( CreateFunctor( pObject, pfnProxied, std::forward<Args>( args )... ) );
	}

	template <class OBJECT_TYPE_PTR, typename FUNCTION_CLASS, typename FUNCTION_RETTYPE, typename... FuncArgs, typename... Args, typename = decltype(std::declval<FUNCTION_RETTYPE(*)(FuncArgs...)>()(std::declval<Args>()...))>
	auto QueueRefCall( OBJECT_TYPE_PTR pObject, FUNCTION_RETTYPE( FUNCTION_CLASS::* pfnProxied )( FuncArgs... ), Args&&... args ) -> std::enable_if_t<std::is_convertible_v<OBJECT_TYPE_PTR, FUNCTION_CLASS*>>
	{
		QueueFunctorInternal( CreateRefCountingFunctor( pObject, pfnProxied, std::forward<Args>( args )... ) );
	}

	template <class OBJECT_TYPE_PTR, typename FUNCTION_CLASS, typename FUNCTION_RETTYPE, typename... FuncArgs, typename... Args, typename = decltype(std::declval<FUNCTION_RETTYPE(*)(FuncArgs...)>()(std::declval<Args>()...))>
	auto QueueRefCall( OBJECT_TYPE_PTR pObject, FUNCTION_RETTYPE( FUNCTION_CLASS::* pfnProxied )( FuncArgs... ) const, Args&&... args ) -> std::enable_if_t<std::is_convertible_v<OBJECT_TYPE_PTR, FUNCTION_CLASS*>>
	{
		QueueFunctorInternal( CreateRefCountingFunctor( pObject, pfnProxied, std::forward<Args>( args )... ) );
	}

private:
	void ExecuteWrapper( int &nDummy )						// to match paralell process function template
	{
		CallQueued();
	}

	void QueueFunctorInternal( CFunctor* pFunctor )
	{
		if ( !m_bNoQueue )
		{
#ifdef _DEBUG
			pFunctor->m_nUserID = m_nCurSerialNumber++;
#endif
			m_queue.PushItem( pFunctor );
		}
		else
		{
			( *pFunctor )();
			pFunctor->Release();
		}
	}

	QUEUE_TYPE m_queue;
	bool m_bNoQueue;
	unsigned m_nCurSerialNumber;
	unsigned m_nBreakSerialNumber;
};

class CCallQueue : public CCallQueueT<>
{
};

//-----------------------------------------------------
// Optional interface that can be bound to concrete CCallQueue
//-----------------------------------------------------

class ICallQueue
{
public:
	void QueueFunctor( CFunctor* pFunctor )
	{
		QueueFunctorInternal( RetAddRef( pFunctor ) );
	}

	template <typename RetType, typename... FuncArgs, typename... Args, typename = decltype(std::declval<RetType(*)(FuncArgs...)>()(std::declval<Args>()...))>
	void QueueCall( RetType( *pfnProxied )( FuncArgs... ), Args&&... args )
	{
		QueueFunctorInternal( CreateFunctor( pfnProxied, std::forward<Args>( args )... ) );
	}

	template <class OBJECT_TYPE_PTR, typename FUNCTION_CLASS, typename FUNCTION_RETTYPE, typename... FuncArgs, typename... Args, typename = decltype(std::declval<FUNCTION_RETTYPE(*)(FuncArgs...)>()(std::declval<Args>()...))>
	auto QueueCall( OBJECT_TYPE_PTR pObject, FUNCTION_RETTYPE( FUNCTION_CLASS::* pfnProxied )( FuncArgs... ), Args&&... args ) -> std::enable_if_t<std::is_convertible_v<OBJECT_TYPE_PTR, FUNCTION_CLASS*>>
	{
		QueueFunctorInternal( CreateFunctor( pObject, pfnProxied, std::forward<Args>( args )... ) );
	}

	template <class OBJECT_TYPE_PTR, typename FUNCTION_CLASS, typename FUNCTION_RETTYPE, typename... FuncArgs, typename... Args, typename = decltype(std::declval<FUNCTION_RETTYPE(*)(FuncArgs...)>()(std::declval<Args>()...))>
	auto QueueCall( OBJECT_TYPE_PTR pObject, FUNCTION_RETTYPE( FUNCTION_CLASS::* pfnProxied )( FuncArgs... ) const, Args&&... args ) -> std::enable_if_t<std::is_convertible_v<OBJECT_TYPE_PTR, FUNCTION_CLASS*>>
	{
		QueueFunctorInternal( CreateFunctor( pObject, pfnProxied, std::forward<Args>( args )... ) );
	}

	template <class OBJECT_TYPE_PTR, typename FUNCTION_CLASS, typename FUNCTION_RETTYPE, typename... FuncArgs, typename... Args, typename = decltype(std::declval<FUNCTION_RETTYPE(*)(FuncArgs...)>()(std::declval<Args>()...))>
	auto QueueRefCall( OBJECT_TYPE_PTR pObject, FUNCTION_RETTYPE( FUNCTION_CLASS::* pfnProxied )( FuncArgs... ), Args&&... args ) -> std::enable_if_t<std::is_convertible_v<OBJECT_TYPE_PTR, FUNCTION_CLASS*>>
	{
		QueueFunctorInternal( CreateRefCountingFunctor( pObject, pfnProxied, std::forward<Args>( args )... ) );
	}

	template <class OBJECT_TYPE_PTR, typename FUNCTION_CLASS, typename FUNCTION_RETTYPE, typename... FuncArgs, typename... Args, typename = decltype(std::declval<FUNCTION_RETTYPE(*)(FuncArgs...)>()(std::declval<Args>()...))>
	auto QueueRefCall( OBJECT_TYPE_PTR pObject, FUNCTION_RETTYPE( FUNCTION_CLASS::* pfnProxied )( FuncArgs... ) const, Args&&... args ) -> std::enable_if_t<std::is_convertible_v<OBJECT_TYPE_PTR, FUNCTION_CLASS*>>
	{
		QueueFunctorInternal( CreateRefCountingFunctor( pObject, pfnProxied, std::forward<Args>( args )... ) );
	}

private:
	virtual void QueueFunctorInternal( CFunctor* pFunctor ) = 0;
};

#endif // CALLQUEUE_H