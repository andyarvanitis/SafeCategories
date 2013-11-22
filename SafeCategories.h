//
//  Copyright (c) 2013, Andy Arvanitis
//  All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without modification,
//  are permitted provided that the following conditions are met:
//  * Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright notice, this
//    list of conditions and the following disclaimer in the documentation and/or
//    other materials provided with the distribution.
//  * Neither the name of the {organization} nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
//  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
//  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
//  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
//  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
//  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
//  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
//  ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
//  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
#import <Foundation/Foundation.h>
#import <objc/runtime.h>

//--------------------------------------------------------------------------------------------------
// Pseudo function that must be called in "category" +load method. The only public interface here.
//--------------------------------------------------------------------------------------------------
#define load_as_safe_category()        \
  __targetClass = [self superclass]; \
  __sourceClass = [self class];

//--------------------------------------------------------------------------------------------------
// Private static variable and function declarations
//--------------------------------------------------------------------------------------------------
static Class __targetClass = Nil;
static Class __sourceClass = Nil;

static NSString* const __RedefinedMethodFormattedErrorMessage =
    @"Safe category: redefined method '%@' found in class '%@'";

static NSArray* get_related_classes(Class baseClass);
static void process_methods(const Class sourceClass, const Class targetClass,
                            void (^method_operation)(Class, Method));

//--------------------------------------------------------------------------------------------------
// Constructor code -- runs afer all +load calls are made
//--------------------------------------------------------------------------------------------------
__attribute__((constructor)) static void pre_run_add_category_methods() {
  @autoreleasepool {
    process_methods(__sourceClass, __targetClass, ^(Class cls, Method method) {
      const SEL sel = method_getName(method);
      NSCAssert(!class_getInstanceMethod(cls, sel), // works with metaclasses too
                __RedefinedMethodFormattedErrorMessage,
                NSStringFromSelector(sel), cls);
      class_addMethod(cls, sel, method_getImplementation(method), method_getTypeEncoding(method));
    });
  }
}

#if !defined(NS_BLOCK_ASSERTIONS) // We don't need any of this if assertions are disabled

//--------------------------------------------------------------------------------------------------
// Destructor code -- runs during program termination
//--------------------------------------------------------------------------------------------------
__attribute__((destructor)) static void post_run_check_category_methods() {
  @autoreleasepool {
    for (Class relatedClass in get_related_classes(__targetClass)) {
      process_methods(__sourceClass, relatedClass, ^(Class cls, Method method) {
        const SEL selector = method_getName(method);
        unsigned int methodsCount = 0;
        Method* methods = class_copyMethodList(cls, &methodsCount); // doesn't search superclasses
        for (unsigned int i = 0; i < methodsCount; i++) {
          if (method_getName(methods[i]) == selector) {
            NSCAssert(method_getImplementation(methods[i]) == method_getImplementation(method),
                      __RedefinedMethodFormattedErrorMessage,
                      NSStringFromSelector(selector), cls);
            break;
          }
        }
        free(methods);
      });
    }
  }
}

//--------------------------------------------------------------------------------------------------
// Look through all of the classes registered with the runtime for super and subclasses.
//--------------------------------------------------------------------------------------------------
static NSArray* get_related_classes(Class baseClass) {
  NSMutableArray* relatedClasses = [NSMutableArray array];
  // First get the base and all its superclasses
  for (Class superClass = baseClass;
       superClass != Nil;
       superClass = class_getSuperclass(superClass)) {
    [relatedClasses addObject: superClass];
  }
  // Now get all subclasses of the base class
  unsigned int count = 0;
  Class* runtimeClasses = objc_copyClassList(&count);
  for (NSInteger i = 0; i < count; i++) {
    Class superClass = runtimeClasses[i];
    do {
      superClass = class_getSuperclass(superClass);
    } while (superClass && superClass != baseClass);

    if (superClass != nil) {
      [relatedClasses addObject: runtimeClasses[i]];
    }
  }
  free(runtimeClasses);
  return relatedClasses;
}

#endif // !defined(NS_BLOCK_ASSERTIONS)

//--------------------------------------------------------------------------------------------------
static void process_methods(const Class sourceClass, const Class targetClass,
                            void (^method_operation)(Class, Method)) {

  void (^iterate_methods)(Class) = ^(Class cls) {
    const BOOL isMetaClass = class_isMetaClass(cls);
    unsigned int count = 0;
    Method* sourceMethods =
        class_copyMethodList(isMetaClass ? object_getClass(sourceClass) : sourceClass, &count);
    for (unsigned int i = 0; i < count; i++) {
      if (!isMetaClass || method_getName(sourceMethods[i]) != @selector(load)) {
        method_operation(cls, sourceMethods[i]);
      }
    }
    free(sourceMethods);
  };
  iterate_methods(object_getClass(targetClass));  // class methods
  iterate_methods(targetClass);                   // instance methods
}

