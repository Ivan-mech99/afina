#include <afina/coroutine/Engine.h>

#include <setjmp.h>
#include <stdio.h>
#include <string.h>

namespace Afina {
namespace Coroutine {

Engine::~Engine() {
   if (StackBottom) {
      delete[] std::get<0>(idle_ctx->Stack);
      delete idle_ctx;
   }
   while (alive) {
      context *ctx = alive;
      delete[] std::get<0>(alive->Stack);
      delete ctx;
      alive = alive->next;
   }
   while (blocked) {
      context *ctx = blocked;
      delete[] std::get<0>(blocked->Stack);
      delete ctx;
      blocked = blocked->next;
   }
}

void Engine::Store(context &ctx) {
   char cur_addr;
   if(&cur_addr > ctx.Low) {
      ctx.Hight = &cur_addr;
   }
   else {
      ctx.Low = &cur_addr;
   }
   size_t mem_needed = ctx.Hight - ctx.Low;
   if (std::get<1>(ctx.Stack) < mem_needed) {
      std::get<1>(ctx.Stack) = mem_needed;
      delete [] std::get<0>(ctx.Stack);
      std::get<0>(ctx.Stack) = new char[mem_needed];
   }
   memcpy(std::get<0>(ctx.Stack), ctx.Low, mem_needed);
}

void Engine::Restore(context &ctx) {
   char cur_addr;
   while (&cur_addr <= ctx.Hight && &cur_addr >= ctx.Low) {
      Restore(ctx);
   }
   memcpy(ctx.Low, std::get<0>(ctx.Stack), std::get<1>(ctx.Stack));
   cur_routine = &ctx;
   longjmp(ctx.Environment, 1);
}

void Engine::yield() {
   context* ctx = alive;
   while (ctx != nullptr && ctx == cur_routine) {
      ctx = ctx -> next;
   }
   if (ctx != nullptr) {
      sched(ctx);
   }  
}

void Engine::sched(void *routine_) {
   if (routine_ == nullptr) {
      yield();
   }
   context *ctx = static_cast<context *>(routine_);
   if (cur_routine == ctx) {
      return;
   }
   if (cur_routine != idle_ctx) {
      if (setjmp(cur_routine->Environment) > 0) {
         return;
      }
      Store(*cur_routine);
   }
   Restore(*ctx);
}

void Engine::block(void *coro) {
   context *blocked_coro;
   if (coro == nullptr) {
      blocked_coro = cur_routine;
   } else {
      blocked_coro = static_cast<context *>(coro);
   }
   if (!blocked_coro || blocked_coro->_is_blocked) {
      return;
   }
   blocked_coro->_is_blocked = true;
   if (alive == blocked_coro) {
      alive = alive->next;
   }
   if (blocked_coro->prev) {
      blocked_coro->prev->next = blocked_coro->next;
   }
   if (blocked_coro->next) {
      blocked_coro->next->prev = blocked_coro->prev;
   }
   blocked_coro->prev = nullptr;
   blocked_coro->next = blocked;
   blocked = blocked_coro;
   if (blocked->next) {
      blocked->next->prev = blocked_coro;
   }
   if (blocked_coro == cur_routine) {
      if (setjmp(cur_routine->Environment) > 0) {
         return;
      }
      Store(*cur_routine);
      Restore(*idle_ctx);
   }
}

void Engine::unblock(void *coro) {
   auto unblocked_coro = static_cast<context *>(coro);
   if (!unblocked_coro || !unblocked_coro->_is_blocked) {
      return;
   }
   unblocked_coro->_is_blocked = false;
   if (blocked == unblocked_coro) {
      blocked = blocked->next;
   }
   if (unblocked_coro->prev) {
      unblocked_coro->prev->next = unblocked_coro->next;
   }
   if (unblocked_coro->next) {
      unblocked_coro->next->prev = unblocked_coro->prev;
   }
   unblocked_coro->prev = nullptr;
   unblocked_coro->next = alive;
   alive = unblocked_coro;
   if (alive->next) {
      alive->next->prev = unblocked_coro;
   }
}

} // namespace Coroutine
} // namespace Afina
