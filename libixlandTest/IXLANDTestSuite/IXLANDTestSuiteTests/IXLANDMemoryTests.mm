//
//  IXLANDMemoryTests.mm
//  IXLANDTestSuiteTests
//
//  Memory syscall tests (mmap, munmap, mprotect)
//

#import <XCTest/XCTest.h>
#import <sys/mman.h>
#import <unistd.h>
#import "IXLANDTestHelpers.h"

@interface IXLANDMemoryTests : XCTestCase
@end

@implementation IXLANDMemoryTests

- (void)setUp {
    [super setUp];
    IXLANDResetLibraryState();
}

- (void)tearDown {
    IXLANDResetLibraryState();
    [super tearDown];
}

// MARK: - mmap Tests

- (void)testMmap_AllocateMemory {
    size_t size = 4096; // One page
    void *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    
    XCTAssertNotEqual(addr, MAP_FAILED, @"mmap should allocate memory");
    XCTAssertNotEqual(addr, NULL, @"mmap should return non-null address");
    
    // Verify we can write to it
    if (addr != MAP_FAILED) {
        char *ptr = (char *)addr;
        ptr[0] = 'A';
        XCTAssertEqual(ptr[0], 'A', @"Should be able to write to mmap'd memory");
        
        // Clean up
        munmap(addr, size);
    }
}

- (void)testMmap_InvalidSize {
    // Try to map 0 bytes
    void *addr = mmap(NULL, 0, PROT_READ, MAP_PRIVATE | MAP_ANON, -1, 0);
    
    // mmap with size 0 returns MAP_FAILED
    XCTAssertEqual(addr, MAP_FAILED, @"mmap with size 0 should fail");
}

- (void)testMmap_InvalidFileDescriptor {
    // Try to map with invalid fd (non-ANON mapping)
    void *addr = mmap(NULL, 4096, PROT_READ, MAP_PRIVATE, 9999, 0);
    
    XCTAssertEqual(addr, MAP_FAILED, @"mmap with invalid fd should fail");
    XCTAssertEqual(errno, EBADF, @"Should set EBADF error");
}

- (void)testMmap_ZeroOffset {
    size_t size = 4096;
    void *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    
    XCTAssertNotEqual(addr, MAP_FAILED, @"mmap with zero offset should succeed");
    
    if (addr != MAP_FAILED) {
        munmap(addr, size);
    }
}

- (void)testMmap_ReadOnly {
    size_t size = 4096;
    void *addr = mmap(NULL, size, PROT_READ, MAP_PRIVATE | MAP_ANON, -1, 0);
    
    XCTAssertNotEqual(addr, MAP_FAILED, @"mmap with PROT_READ should succeed");
    
    if (addr != MAP_FAILED) {
        munmap(addr, size);
    }
}

- (void)testMmap_WriteOnly {
    size_t size = 4096;
    void *addr = mmap(NULL, size, PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    
    XCTAssertNotEqual(addr, MAP_FAILED, @"mmap with PROT_WRITE should succeed");
    
    if (addr != MAP_FAILED) {
        char *ptr = (char *)addr;
        ptr[0] = 'X';
        XCTAssertEqual(ptr[0], 'X', @"Should be able to write to PROT_WRITE memory");
        munmap(addr, size);
    }
}

- (void)testMmap_LargeAllocation {
    size_t size = 1024 * 1024; // 1MB
    size_t before = [IXLANDMemoryTracker currentResidentSize];
    
    void *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    
    XCTAssertNotEqual(addr, MAP_FAILED, @"mmap should allocate large memory block");
    
    if (addr != MAP_FAILED) {
        // Write to ensure pages are allocated
        memset(addr, 0xAB, size);
        
        size_t after = [IXLANDMemoryTracker currentResidentSize];
        NSLog(@"Memory before: %zu MB, after: %zu MB", before / (1024*1024), after / (1024*1024));
        
        munmap(addr, size);
        
        IXLANDAssertNoMemoryLeak(@"Large mmap allocation should not leak");
    }
}

- (void)testMmap_MultipleAllocations {
    void *addrs[10];
    size_t size = 4096;
    
    // Allocate multiple blocks
    for (int i = 0; i < 10; i++) {
        addrs[i] = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
        XCTAssertNotEqual(addrs[i], MAP_FAILED, @"Allocation %d should succeed", i);
        
        if (addrs[i] != MAP_FAILED) {
            // Write unique value
            ((int *)addrs[i])[0] = i;
        }
    }
    
    // Verify all values
    for (int i = 0; i < 10; i++) {
        if (addrs[i] != MAP_FAILED) {
            XCTAssertEqual(((int *)addrs[i])[0], i, @"Value at allocation %d should be preserved", i);
        }
    }
    
    // Free all
    for (int i = 0; i < 10; i++) {
        if (addrs[i] != MAP_FAILED) {
            munmap(addrs[i], size);
        }
    }
    
    IXLANDAssertNoMemoryLeak(@"Multiple mmap allocations should not leak");
}

// MARK: - munmap Tests

- (void)testMunmap_Success {
    size_t size = 4096;
    void *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    
    XCTAssertNotEqual(addr, MAP_FAILED, @"mmap should succeed");
    
    if (addr != MAP_FAILED) {
        int result = munmap(addr, size);
        XCTAssertEqual(result, 0, @"munmap should succeed");
    }
}

- (void)testMunmap_InvalidAddress {
    // Try to unmap invalid address
    int result = munmap((void *)0x1, 4096);
    
    // On iOS, this might fail or be silently ignored depending on implementation
    // We just verify it doesn't crash
    NSLog(@"munmap result: %d (errno: %d)", result, errno);
}

- (void)testMunmap_ZeroSize {
    // Allocate memory
    size_t size = 4096;
    void *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    
    XCTAssertNotEqual(addr, MAP_FAILED, @"mmap should succeed");
    
    if (addr != MAP_FAILED) {
        // Try unmap with 0 size - behavior varies by implementation
        int result = munmap(addr, 0);
        NSLog(@"munmap with size 0 result: %d", result);
        
        // Clean up properly
        munmap(addr, size);
    }
}

// MARK: - mprotect Tests

- (void)testMprotect_ChangeToReadOnly {
    size_t size = 4096;
    void *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    
    XCTAssertNotEqual(addr, MAP_FAILED, @"mmap should succeed");
    
    if (addr != MAP_FAILED) {
        // Write some data
        char *ptr = (char *)addr;
        ptr[0] = 'A';
        
        // Change to read-only
        int result = mprotect(addr, size, PROT_READ);
        XCTAssertEqual(result, 0, @"mprotect to PROT_READ should succeed");
        
        // Verify we can still read
        XCTAssertEqual(ptr[0], 'A', @"Should still be able to read");
        
        // Restore write permission for cleanup
        mprotect(addr, size, PROT_READ | PROT_WRITE);
        
        munmap(addr, size);
    }
}

- (void)testMprotect_ChangeToReadWrite {
    size_t size = 4096;
    void *addr = mmap(NULL, size, PROT_READ, MAP_PRIVATE | MAP_ANON, -1, 0);
    
    XCTAssertNotEqual(addr, MAP_FAILED, @"mmap with PROT_READ should succeed");
    
    if (addr != MAP_FAILED) {
        // Change to read-write
        int result = mprotect(addr, size, PROT_READ | PROT_WRITE);
        XCTAssertEqual(result, 0, @"mprotect to PROT_READ | PROT_WRITE should succeed");
        
        // Now try to write
        char *ptr = (char *)addr;
        ptr[0] = 'B';
        XCTAssertEqual(ptr[0], 'B', @"Should be able to write after mprotect");
        
        munmap(addr, size);
    }
}

- (void)testMprotect_NoAccess {
    size_t size = 4096;
    void *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    
    XCTAssertNotEqual(addr, MAP_FAILED, @"mmap should succeed");
    
    if (addr != MAP_FAILED) {
        // Change to no access
        int result = mprotect(addr, size, PROT_NONE);
        XCTAssertEqual(result, 0, @"mprotect to PROT_NONE should succeed");
        
        // Restore for cleanup
        mprotect(addr, size, PROT_READ | PROT_WRITE);
        
        munmap(addr, size);
    }
}

- (void)testMprotect_InvalidAddress {
    int result = mprotect((void *)0x1, 4096, PROT_READ);
    
    // Should fail with EINVAL
    XCTAssertEqual(result, -1, @"mprotect with invalid address should fail");
    XCTAssertEqual(errno, EINVAL, @"Should set EINVAL error");
}

// MARK: - Integration Tests

- (void)testMemoryStress_MmapMunmap {
    size_t initialMemory = [IXLANDMemoryTracker currentResidentSize];
    size_t size = 4096;
    
    // Allocate and free 1000 times
    for (int i = 0; i < 1000; i++) {
        void *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
        XCTAssertNotEqual(addr, MAP_FAILED, @"Allocation %d should succeed", i);
        
        if (addr != MAP_FAILED) {
            // Write to ensure page is committed
            ((int *)addr)[0] = i;
            
            // Free
            int result = munmap(addr, size);
            XCTAssertEqual(result, 0, @"Free %d should succeed", i);
        }
    }
    
    // Allow time for memory to be reclaimed
    [NSThread sleepForTimeInterval:0.1];
    
    size_t finalMemory = [IXLANDMemoryTracker currentResidentSize];
    ssize_t diff = (ssize_t)finalMemory - (ssize_t)initialMemory;
    
    NSLog(@"Memory stress test: initial=%zu, final=%zu, diff=%zd", initialMemory, finalMemory, diff);
    
    // Memory should be roughly back to initial (allow 10% variance)
    XCTAssertLessThan(fabs(diff), initialMemory * 0.1,
                      @"Memory should return to near initial after stress test");
}

- (void)testMemoryStress_MprotectChanges {
    size_t size = 4096;
    void *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    
    XCTAssertNotEqual(addr, MAP_FAILED, @"mmap should succeed");
    
    if (addr != MAP_FAILED) {
        char *ptr = (char *)addr;
        
        // Rapidly change protection 1000 times
        for (int i = 0; i < 1000; i++) {
            int prot;
            if (i % 3 == 0) {
                prot = PROT_READ | PROT_WRITE;
            } else if (i % 3 == 1) {
                prot = PROT_READ;
            } else {
                prot = PROT_WRITE;
            }
            
            int result = mprotect(addr, size, prot);
            XCTAssertEqual(result, 0, @"mprotect iteration %d should succeed", i);
            
            if ((prot & PROT_WRITE) != 0) {
                ptr[i % size] = (char)i;
            }
        }
        
        munmap(addr, size);
        
        IXLANDAssertNoMemoryLeak(@"mprotect stress test should not leak");
    }
}

- (void)testMemory_MultipleSizes {
    size_t sizes[] = {4096, 8192, 16384, 32768, 65536, 131072, 262144, 524288, 1048576};
    size_t numSizes = sizeof(sizes) / sizeof(sizes[0]);
    
    for (size_t i = 0; i < numSizes; i++) {
        void *addr = mmap(NULL, sizes[i], PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
        XCTAssertNotEqual(addr, MAP_FAILED, @"mmap size %zu should succeed", sizes[i]);
        
        if (addr != MAP_FAILED) {
            // Touch first and last page
            char *ptr = (char *)addr;
            ptr[0] = 'A';
            ptr[sizes[i] - 1] = 'Z';
            
            XCTAssertEqual(ptr[0], 'A', @"First byte should be writeable");
            XCTAssertEqual(ptr[sizes[i] - 1], 'Z', @"Last byte should be writeable");
            
            munmap(addr, sizes[i]);
        }
    }
    
    IXLANDAssertNoMemoryLeak(@"Multiple size allocations should not leak");
}

@end
