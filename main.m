#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
#import <mach/mach.h>
#import <mach/mach_host.h>
#import <mach/processor_info.h>
#import <mach/vm_statistics.h>
#import <sys/mount.h>
#import <sys/resource.h>
#import <sys/sysctl.h>
#import <os/proc.h>
#import <net/if.h>
#import <net/if_var.h>
#import <net/if_dl.h>
#import <ifaddrs.h>
#import <signal.h>
#import <unistd.h>
#import <math.h>
#import <stdlib.h>
#import <string.h>
#import <time.h>

// libproc is part of iOS libSystem, but Apple does not ship its header in the
// public iOS SDK. Keep the tiny ABI surface we use here local and read-only.
extern int proc_listpids(uint32_t type, uint32_t typeinfo, void *buffer, int buffersize);
extern int proc_pidinfo(int pid, int flavor, uint64_t arg, void *buffer, int buffersize);
extern int proc_pid_rusage(int pid, int flavor, rusage_info_t *buffer);

enum { ITopProcAllPIDs = 1, ITopProcPIDTaskAllInfo = 2, ITopProcPIDTaskInfo = 4 };

typedef struct {
    uint64_t virtualSize;
    uint64_t residentSize;
    uint64_t totalUser;
    uint64_t totalSystem;
    uint64_t threadsUser;
    uint64_t threadsSystem;
    int32_t policy;
    int32_t faults;
    int32_t pageins;
    int32_t cowFaults;
    int32_t messagesSent;
    int32_t messagesReceived;
    int32_t machSyscalls;
    int32_t unixSyscalls;
    int32_t contextSwitches;
    int32_t threadCount;
    int32_t runningThreads;
    int32_t priority;
} ITopProcTaskInfo;

typedef struct {
    uint32_t flags;
    uint32_t status;
    uint32_t exitStatus;
    uint32_t pid;
    uint32_t parentPID;
    uid_t uid;
    gid_t gid;
    uid_t realUID;
    gid_t realGID;
    uid_t savedUID;
    gid_t savedGID;
    uint32_t reserved;
    char command[16];
    char name[32];
    uint32_t openFiles;
    uint32_t processGroup;
    uint32_t processJobCount;
    uint32_t ttyDevice;
    uint32_t ttyProcessGroup;
    int32_t nice;
    uint64_t startSeconds;
    uint64_t startMicroseconds;
} ITopProcBSDInfo;

typedef struct {
    ITopProcBSDInfo bsd;
    ITopProcTaskInfo task;
} ITopProcTaskAllInfo;

@interface ITopProcess : NSObject
@property (nonatomic) pid_t pid;
@property (nonatomic) uid_t uid;
@property (nonatomic) uint64_t virtualBytes;
@property (nonatomic) uint64_t residentBytes;
@property (nonatomic) uint64_t cpuTimeNanoseconds;
@property (nonatomic) double cpu;
@property (nonatomic) int32_t threads;
@property (nonatomic) int32_t runningThreads;
@property (nonatomic) int32_t priority;
@property (nonatomic) int32_t nice;
@property (nonatomic) uint64_t footprintBytes;
@property (nonatomic) uint64_t peakFootprintBytes;
@property (nonatomic) double energyPowerMilliwatts;
@property (nonatomic) BOOL energyAvailable;
@property (nonatomic) double faultsPerSecond;
@property (nonatomic) double pageinsPerSecond;
@property (nonatomic) double contextSwitchesPerSecond;
@property (nonatomic) double syscallsPerSecond;
@property (nonatomic, copy) NSString *name;
@end
@implementation ITopProcess
@end

@interface ITopProcHistory : NSObject
@property (nonatomic) uint64_t cpuTicks;
@property (nonatomic) int32_t faults;
@property (nonatomic) int32_t pageins;
@property (nonatomic) int32_t contextSwitches;
@property (nonatomic) int32_t machSyscalls;
@property (nonatomic) int32_t unixSyscalls;
@property (nonatomic) uint64_t footprintBytes;
@property (nonatomic) uint64_t peakFootprintBytes;
@property (nonatomic) uint64_t energyNanojoules;
@property (nonatomic) NSTimeInterval energySampleTime;
@property (nonatomic) double energyPowerMilliwatts;
@property (nonatomic) BOOL energyAvailable;
@end
@implementation ITopProcHistory
@end

@interface ITopSnapshot : NSObject
@property (nonatomic) double cpu;
@property (nonatomic, copy) NSArray<NSNumber *> *coreUsage;
@property (nonatomic) uint64_t memoryUsed;
@property (nonatomic) uint64_t memoryTotal;
@property (nonatomic) uint64_t memoryWired;
@property (nonatomic) uint64_t memoryCompressed;
@property (nonatomic) uint64_t appAvailableMemory;
@property (nonatomic) uint64_t diskUsed;
@property (nonatomic) uint64_t diskTotal;
@property (nonatomic) double wifiReceiveBytesPerSecond;
@property (nonatomic) double wifiSendBytesPerSecond;
@property (nonatomic) double cellularReceiveBytesPerSecond;
@property (nonatomic) double cellularSendBytesPerSecond;
@property (nonatomic) BOOL networkAvailable;
@property (nonatomic) BOOL energyAvailable;
@property (nonatomic) double collectionMilliseconds;
@property (nonatomic, copy) NSString *thermalState;
@property (nonatomic) NSUInteger processCount;
@property (nonatomic) BOOL processAccess;
@property (nonatomic, copy) NSArray<ITopProcess *> *processes;
@property (nonatomic, copy) NSString *machine;
@property (nonatomic, copy) NSString *osVersion;
@property (nonatomic) NSTimeInterval uptime;
@end
@implementation ITopSnapshot
@end

@interface ITopMonitor : NSObject {
    natural_t _previousCoreCount;
    integer_t *_previousCoreTicks;
    BOOL _hasPreviousCPU;
    NSTimeInterval _previousProcessSampleTime;
    double _nanosecondsPerMachTick;
    NSTimeInterval _lastRusageSampleTime;
    NSTimeInterval _previousNetworkSampleTime;
    NSMutableDictionary<NSNumber *, ITopProcHistory *> *_previousProcessStats;
    NSMutableDictionary<NSString *, NSDictionary<NSString *, NSNumber *> *> *_previousInterfaces;
}
- (ITopSnapshot *)collect;
@end

static NSString *ITopSysctlString(const char *key) {
    size_t size = 0;
    if (sysctlbyname(key, NULL, &size, NULL, 0) != 0 || size == 0) return @"unknown";
    char *buffer = calloc(1, size + 1);
    if (!buffer) return @"unknown";
    NSString *result = @"unknown";
    if (sysctlbyname(key, buffer, &size, NULL, 0) == 0) {
        NSString *value = [NSString stringWithUTF8String:buffer];
        if (value.length) result = value;
    }
    free(buffer);
    return result;
}

@implementation ITopMonitor
- (instancetype)init {
    self = [super init];
    if (self) {
        _previousProcessStats = [NSMutableDictionary dictionary];
        _previousInterfaces = [NSMutableDictionary dictionary];
        mach_timebase_info_data_t timebase = {0};
        if (mach_timebase_info(&timebase) == KERN_SUCCESS && timebase.denom)
            _nanosecondsPerMachTick = (double)timebase.numer / (double)timebase.denom;
        else
            _nanosecondsPerMachTick = 1.0;
    }
    return self;
}

static uint64_t ITopDelta32(uint32_t current, uint32_t previous) {
    return current >= previous ? (uint64_t)(current - previous) :
        (uint64_t)UINT32_MAX - previous + current + 1;
}

- (void)sampleNetworkInto:(ITopSnapshot *)snapshot now:(NSTimeInterval)now {
    struct ifaddrs *interfaces = NULL;
    if (getifaddrs(&interfaces) != 0 || !interfaces) {
        snapshot.networkAvailable = NO;
        return;
    }
    NSTimeInterval elapsed = _previousNetworkSampleTime > 0 ? now - _previousNetworkSampleTime : 0;
    NSMutableDictionary<NSString *, NSDictionary<NSString *, NSNumber *> *> *next = [NSMutableDictionary dictionary];
    uint64_t wifiRX = 0, wifiTX = 0, cellRX = 0, cellTX = 0;
    BOOL sawWiFi = NO, sawCellular = NO;
    for (struct ifaddrs *item = interfaces; item; item = item->ifa_next) {
        if (!item->ifa_name || !item->ifa_addr || item->ifa_addr->sa_family != AF_LINK || !item->ifa_data) continue;
        const struct if_data *data = (const struct if_data *)item->ifa_data;
        NSString *name = [NSString stringWithUTF8String:item->ifa_name];
        if (!name.length) continue;
        uint32_t rx = data->ifi_ibytes;
        uint32_t tx = data->ifi_obytes;
        NSDictionary *previous = _previousInterfaces[name];
        if (previous && elapsed > 0.0001) {
            uint64_t rxDelta = ITopDelta32(rx, [previous[@"rx"] unsignedIntValue]);
            uint64_t txDelta = ITopDelta32(tx, [previous[@"tx"] unsignedIntValue]);
            double rxRate = (double)rxDelta / elapsed;
            double txRate = (double)txDelta / elapsed;
            if ([name hasPrefix:@"en"]) { wifiRX += (uint64_t)rxRate; wifiTX += (uint64_t)txRate; }
            else if ([name hasPrefix:@"pdp_ip"]) { cellRX += (uint64_t)rxRate; cellTX += (uint64_t)txRate; }
        }
        if ([name hasPrefix:@"en"]) sawWiFi = YES;
        if ([name hasPrefix:@"pdp_ip"]) sawCellular = YES;
        next[name] = @{@"rx": @(rx), @"tx": @(tx)};
    }
    freeifaddrs(interfaces);
    _previousInterfaces = next;
    _previousNetworkSampleTime = now;
    snapshot.networkAvailable = sawWiFi || sawCellular;
    snapshot.wifiReceiveBytesPerSecond = wifiRX;
    snapshot.wifiSendBytesPerSecond = wifiTX;
    snapshot.cellularReceiveBytesPerSecond = cellRX;
    snapshot.cellularSendBytesPerSecond = cellTX;
}

- (NSArray<NSNumber *> *)sampleCoresWithOverall:(double *)overall {
    natural_t coreCount = 0;
    processor_info_array_t info = NULL;
    mach_msg_type_number_t infoCount = 0;
    kern_return_t result = host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO,
                                               &coreCount, &info, &infoCount);
    if (result != KERN_SUCCESS || !info || coreCount == 0) {
        if (info) vm_deallocate(mach_task_self(), (vm_address_t)info,
                                (vm_size_t)infoCount * sizeof(integer_t));
        *overall = 0;
        return @[];
    }

    NSMutableArray<NSNumber *> *usage = [NSMutableArray arrayWithCapacity:coreCount];
    uint64_t allBusy = 0, allTicks = 0;
    BOOL canCompare = _hasPreviousCPU && _previousCoreCount == coreCount && _previousCoreTicks;
    for (natural_t core = 0; core < coreCount; core++) {
        uint64_t busy = 0, total = 0;
        for (int state = 0; state < CPU_STATE_MAX; state++) {
            NSUInteger index = (NSUInteger)core * CPU_STATE_MAX + (NSUInteger)state;
            uint64_t current = (uint32_t)info[index];
            uint64_t previous = canCompare ? (uint32_t)_previousCoreTicks[index] : current;
            uint64_t delta = current >= previous ? current - previous : 0;
            total += delta;
            if (state != CPU_STATE_IDLE) busy += delta;
            if (canCompare) _previousCoreTicks[index] = info[index];
        }
        double percent = total ? 100.0 * (double)busy / (double)total : 0.0;
        [usage addObject:@(percent)];
        allBusy += busy;
        allTicks += total;
    }

    if (!canCompare) {
        free(_previousCoreTicks);
        _previousCoreTicks = calloc((size_t)coreCount * CPU_STATE_MAX, sizeof(integer_t));
        if (_previousCoreTicks) {
            memcpy(_previousCoreTicks, info,
                   (size_t)coreCount * CPU_STATE_MAX * sizeof(integer_t));
            _previousCoreCount = coreCount;
        }
    }
    _hasPreviousCPU = YES;
    *overall = allTicks ? 100.0 * (double)allBusy / (double)allTicks : 0.0;
    vm_deallocate(mach_task_self(), (vm_address_t)info,
                  (vm_size_t)infoCount * sizeof(integer_t));
    return usage;
}

- (NSArray<ITopProcess *> *)sampleProcesses:(NSUInteger *)totalCount access:(BOOL *)access {
    const int capacity = 8192;
    pid_t *pids = calloc((size_t)capacity, sizeof(pid_t));
    if (!pids) {
        *totalCount = 0;
        *access = NO;
        return @[];
    }
    int byteCount = proc_listpids(ITopProcAllPIDs, 0, pids, capacity * (int)sizeof(pid_t));
    if (byteCount <= 0) {
        free(pids);
        *totalCount = 0;
        *access = NO;
        return @[];
    }
    NSUInteger pidCount = MIN((NSUInteger)(byteCount / (int)sizeof(pid_t)), (NSUInteger)capacity);
    *totalCount = pidCount;
    *access = YES;

    NSTimeInterval now = NSProcessInfo.processInfo.systemUptime;
    NSTimeInterval elapsed = _previousProcessSampleTime > 0 ? now - _previousProcessSampleTime : 0;
    BOOL sampleRusage = _lastRusageSampleTime == 0 || now - _lastRusageSampleTime >= 5.0;
    NSMutableDictionary<NSNumber *, ITopProcHistory *> *nextStats = [NSMutableDictionary dictionaryWithCapacity:pidCount];
    NSMutableArray<ITopProcess *> *rows = [NSMutableArray arrayWithCapacity:MIN(pidCount, (NSUInteger)512)];

    for (NSUInteger i = 0; i < pidCount; i++) {
        pid_t pid = pids[i];
        if (pid <= 0) continue;
        ITopProcTaskAllInfo allInfo = {0};
        int taskBytes = proc_pidinfo(pid, ITopProcPIDTaskAllInfo, 0, &allInfo, (int)sizeof(allInfo));
        if (taskBytes < (int)sizeof(allInfo)) continue;
        ITopProcTaskInfo task = allInfo.task;
        uid_t uid = allInfo.bsd.uid;

        char nameBuffer[33] = {0};
        memcpy(nameBuffer, allInfo.bsd.name, sizeof(allInfo.bsd.name));
        if (!nameBuffer[0]) memcpy(nameBuffer, allInfo.bsd.command, sizeof(allInfo.bsd.command));
        if (!nameBuffer[0]) snprintf(nameBuffer, sizeof(nameBuffer), "pid-%d", pid);
        NSString *name = [NSString stringWithUTF8String:nameBuffer];
        if (!name.length) name = [NSString stringWithFormat:@"pid-%d", pid];

        uint64_t cpuTime = task.totalUser + task.totalSystem;
        NSNumber *key = @(pid);
        ITopProcHistory *prior = _previousProcessStats[key];
        double cpu = 0;
        if (prior && elapsed > 0.0001 && cpuTime >= prior.cpuTicks) {
            double cpuNanoseconds = (double)(cpuTime - prior.cpuTicks) * _nanosecondsPerMachTick;
            cpu = cpuNanoseconds / (elapsed * 10000000.0);
            cpu = MAX(0, MIN(cpu, 800));
        }

        ITopProcHistory *current = [ITopProcHistory new];
        current.cpuTicks = cpuTime;
        current.faults = task.faults;
        current.pageins = task.pageins;
        current.contextSwitches = task.contextSwitches;
        current.machSyscalls = task.machSyscalls;
        current.unixSyscalls = task.unixSyscalls;
        double counterInterval = MAX(0.001, elapsed);
        double faultsPerSecond = prior && task.faults >= prior.faults ? (task.faults - prior.faults) / counterInterval : 0;
        double pageinsPerSecond = prior && task.pageins >= prior.pageins ? (task.pageins - prior.pageins) / counterInterval : 0;
        double switchesPerSecond = prior && task.contextSwitches >= prior.contextSwitches ?
            (task.contextSwitches - prior.contextSwitches) / counterInterval : 0;
        int64_t syscallDelta = prior ? (int64_t)task.machSyscalls + task.unixSyscalls - prior.machSyscalls - prior.unixSyscalls : 0;
        double syscallsPerSecond = MAX(0, syscallDelta) / counterInterval;

        if (sampleRusage) {
            struct rusage_info_v6 usage6 = {0};
            if (proc_pid_rusage(pid, RUSAGE_INFO_V6, (rusage_info_t *)&usage6) == 0) {
                current.footprintBytes = usage6.ri_phys_footprint;
                current.peakFootprintBytes = usage6.ri_lifetime_max_phys_footprint;
                current.energyNanojoules = usage6.ri_energy_nj;
                current.energySampleTime = now;
                current.energyAvailable = YES;
                if (prior.energyAvailable && now > prior.energySampleTime && usage6.ri_energy_nj >= prior.energyNanojoules) {
                    double elapsedEnergy = now - prior.energySampleTime;
                    current.energyPowerMilliwatts = (double)(usage6.ri_energy_nj - prior.energyNanojoules) /
                        elapsedEnergy / 1000000.0;
                }
            } else {
                struct rusage_info_v4 usage4 = {0};
                if (proc_pid_rusage(pid, RUSAGE_INFO_V4, (rusage_info_t *)&usage4) == 0) {
                    current.footprintBytes = usage4.ri_phys_footprint;
                    current.peakFootprintBytes = usage4.ri_lifetime_max_phys_footprint;
                }
                if (prior) {
                    current.energyNanojoules = prior.energyNanojoules;
                    current.energySampleTime = prior.energySampleTime;
                    current.energyPowerMilliwatts = prior.energyPowerMilliwatts;
                    current.energyAvailable = prior.energyAvailable;
                }
            }
        } else if (prior) {
            current.footprintBytes = prior.footprintBytes;
            current.peakFootprintBytes = prior.peakFootprintBytes;
            current.energyNanojoules = prior.energyNanojoules;
            current.energySampleTime = prior.energySampleTime;
            current.energyPowerMilliwatts = prior.energyPowerMilliwatts;
            current.energyAvailable = prior.energyAvailable;
        }
        nextStats[key] = current;

        ITopProcess *process = [ITopProcess new];
        process.pid = pid;
        process.uid = uid;
        process.name = name;
        process.cpu = cpu;
        process.cpuTimeNanoseconds = (uint64_t)((double)cpuTime * _nanosecondsPerMachTick);
        process.virtualBytes = task.virtualSize;
        process.residentBytes = task.residentSize;
        process.threads = task.threadCount;
        process.runningThreads = task.runningThreads;
        process.priority = task.priority;
        process.nice = allInfo.bsd.nice;
        process.footprintBytes = current.footprintBytes;
        process.peakFootprintBytes = current.peakFootprintBytes;
        process.energyPowerMilliwatts = current.energyPowerMilliwatts;
        process.energyAvailable = current.energyAvailable;
        process.faultsPerSecond = faultsPerSecond;
        process.pageinsPerSecond = pageinsPerSecond;
        process.contextSwitchesPerSecond = switchesPerSecond;
        process.syscallsPerSecond = syscallsPerSecond;
        [rows addObject:process];
    }
    free(pids);
    _previousProcessStats = nextStats;
    _previousProcessSampleTime = now;
    if (sampleRusage) _lastRusageSampleTime = now;
    return rows;
}

- (ITopSnapshot *)collect {
    NSTimeInterval collectionStart = NSProcessInfo.processInfo.systemUptime;
    ITopSnapshot *snapshot = [ITopSnapshot new];
    snapshot.machine = ITopSysctlString("hw.machine");
    snapshot.osVersion = ITopSysctlString("kern.osproductversion");
    if ([snapshot.osVersion isEqualToString:@"unknown"]) snapshot.osVersion = @"iOS";
    snapshot.uptime = NSProcessInfo.processInfo.systemUptime;
    switch (NSProcessInfo.processInfo.thermalState) {
        case NSProcessInfoThermalStateNominal: snapshot.thermalState = @"NOMINAL"; break;
        case NSProcessInfoThermalStateFair: snapshot.thermalState = @"FAIR"; break;
        case NSProcessInfoThermalStateSerious: snapshot.thermalState = @"SERIOUS"; break;
        case NSProcessInfoThermalStateCritical: snapshot.thermalState = @"CRITICAL"; break;
        default: snapshot.thermalState = @"UNKNOWN"; break;
    }
    snapshot.appAvailableMemory = os_proc_available_memory();
    double overallCPU = 0;
    snapshot.coreUsage = [self sampleCoresWithOverall:&overallCPU];
    snapshot.cpu = overallCPU;

    vm_statistics64_data_t vm = {0};
    mach_msg_type_number_t vmCount = HOST_VM_INFO64_COUNT;
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vm, &vmCount) == KERN_SUCCESS) {
        vm_size_t pageSize = 0;
        host_page_size(mach_host_self(), &pageSize);
        uint64_t totalPages = (uint64_t)vm.active_count + vm.inactive_count + vm.wire_count +
                              vm.free_count + vm.speculative_count + vm.compressor_page_count;
        uint64_t usedPages = (uint64_t)vm.active_count + vm.wire_count + vm.compressor_page_count;
        uint64_t physical = 0;
        size_t physicalSize = sizeof(physical);
        if (sysctlbyname("hw.memsize", &physical, &physicalSize, NULL, 0) != 0) physical = 0;
        snapshot.memoryTotal = physical ? physical : totalPages * pageSize;
        snapshot.memoryUsed = MIN(usedPages * pageSize, snapshot.memoryTotal);
        snapshot.memoryWired = (uint64_t)vm.wire_count * pageSize;
        snapshot.memoryCompressed = (uint64_t)vm.compressor_page_count * pageSize;
    }

    struct statfs diskInfo;
    if (statfs("/", &diskInfo) == 0) {
        snapshot.diskTotal = (uint64_t)diskInfo.f_blocks * (uint64_t)diskInfo.f_bsize;
        uint64_t freeBytes = (uint64_t)diskInfo.f_bavail * (uint64_t)diskInfo.f_bsize;
        snapshot.diskUsed = snapshot.diskTotal > freeBytes ? snapshot.diskTotal - freeBytes : 0;
    }

    NSUInteger processCount = 0;
    BOOL processAccess = NO;
    snapshot.processes = [self sampleProcesses:&processCount access:&processAccess];
    snapshot.processCount = processCount;
    snapshot.processAccess = processAccess;
    [self sampleNetworkInto:snapshot now:NSProcessInfo.processInfo.systemUptime];
    snapshot.collectionMilliseconds = (NSProcessInfo.processInfo.systemUptime - collectionStart) * 1000.0;
    for (ITopProcess *process in snapshot.processes) {
        if (process.energyAvailable) { snapshot.energyAvailable = YES; break; }
    }
    return snapshot;
}

- (void)dealloc {
    free(_previousCoreTicks);
}
@end

#include "terminal_ui.inc"

@interface ITopViewController : UIViewController <ITopCanvasDelegate>
@end

@implementation ITopViewController {
    ITopCanvas *_canvas;
    ITopMonitor *_monitor;
    dispatch_queue_t _monitorQueue;
    NSTimer *_sampleTimer;
    CADisplayLink *_displayLink;
    CFTimeInterval _fpsWindowStart;
    NSUInteger _fpsFrameCount;
    BOOL _sampling;
}

- (void)loadView {
    _canvas = [[ITopCanvas alloc] initWithFrame:UIScreen.mainScreen.bounds];
    _canvas.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    _canvas.delegate = self;
    self.view = _canvas;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    _monitor = [ITopMonitor new];
    _monitorQueue = dispatch_queue_create("com.itop.metrics", DISPATCH_QUEUE_SERIAL);
    UIDevice.currentDevice.batteryMonitoringEnabled = YES;
    [self updateBattery];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(appDidBackground)
                                                 name:UIApplicationDidEnterBackgroundNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(appWillForeground)
                                                 name:UIApplicationWillEnterForegroundNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(keyboardFrameChanged:)
                                                 name:UIKeyboardWillChangeFrameNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(keyboardHidden)
                                                 name:UIKeyboardWillHideNotification object:nil];
}

- (void)viewDidAppear:(BOOL)animated {
    [super viewDidAppear:animated];
    [self becomeFirstResponder];
    if (!_displayLink) {
        _displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(onDisplayFrame:)];
        NSInteger maximum = MAX(30, UIScreen.mainScreen.maximumFramesPerSecond);
        _displayLink.preferredFrameRateRange = CAFrameRateRangeMake(30, (float)maximum, (float)maximum);
        [_displayLink addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
    }
    [self startSampling];
}

- (void)viewWillDisappear:(BOOL)animated {
    [super viewWillDisappear:animated];
    [_displayLink invalidate];
    _displayLink = nil;
    [self stopSampling];
}

- (void)dealloc {
    [_displayLink invalidate];
    [_sampleTimer invalidate];
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    UIDevice.currentDevice.batteryMonitoringEnabled = NO;
}

- (BOOL)prefersStatusBarHidden { return YES; }
- (BOOL)canBecomeFirstResponder { return YES; }

- (NSArray<UIKeyCommand *> *)keyCommands {
    if (_canvas.isSearching) return @[];
    if (_canvas.isJumping) {
        // Jump prompt is modal: Esc cancels, but paging still works so the
        // user can review rows while choosing a PID; digits flow to insertText.
        return @[
            [UIKeyCommand keyCommandWithInput:ITopEscapeKey() modifierFlags:0 action:@selector(keyEscape)],
            [UIKeyCommand keyCommandWithInput:UIKeyInputPageUp modifierFlags:0 action:@selector(keyPageUp)],
            [UIKeyCommand keyCommandWithInput:UIKeyInputPageDown modifierFlags:0 action:@selector(keyPageDown)],
            [UIKeyCommand keyCommandWithInput:UIKeyInputHome modifierFlags:0 action:@selector(keyHome)],
            [UIKeyCommand keyCommandWithInput:UIKeyInputEnd modifierFlags:0 action:@selector(keyEnd)]
        ];
    }
    return @[
        [UIKeyCommand keyCommandWithInput:@"s" modifierFlags:0 action:@selector(keySort)],
        [UIKeyCommand keyCommandWithInput:@"f" modifierFlags:0 action:@selector(keyFilter)],
        [UIKeyCommand keyCommandWithInput:@"j" modifierFlags:0 action:@selector(keyJump)],
        [UIKeyCommand keyCommandWithInput:@"r" modifierFlags:0 action:@selector(keyRefresh)],
        [UIKeyCommand keyCommandWithInput:@"h" modifierFlags:0 action:@selector(keyHelp)],
        [UIKeyCommand keyCommandWithInput:ITopEscapeKey() modifierFlags:0 action:@selector(keyEscape)],
        [UIKeyCommand keyCommandWithInput:@" " modifierFlags:0 action:@selector(keyPause)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputUpArrow modifierFlags:0 action:@selector(keyUp)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputDownArrow modifierFlags:0 action:@selector(keyDown)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputPageUp modifierFlags:0 action:@selector(keyPageUp)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputPageDown modifierFlags:0 action:@selector(keyPageDown)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputHome modifierFlags:0 action:@selector(keyHome)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputEnd modifierFlags:0 action:@selector(keyEnd)]
    ];
}
- (void)keySort { [self itopChangeSort]; }
- (void)keyFilter { [self itopChangeFilter]; }
- (void)keyJump { [self itopBeginJump]; }
- (void)keyRefresh { [self itopRefresh]; }
- (void)keyHelp { [self itopShowHelp]; }
- (void)keyEscape {
    if (_canvas.isSearching) [_canvas cancelSearch];
    else if (_canvas.isJumping) [_canvas cancelJump];
    else _canvas.helpMode = NO;
    [_canvas setNeedsDisplay];
}
- (void)keyPause { [self itopTogglePause]; }
- (void)keyUp { [_canvas moveSelectionBy:-1]; }
- (void)keyDown { [_canvas moveSelectionBy:1]; }
- (void)keyPageUp { [_canvas scrollPageBy:-1]; }
- (void)keyPageDown { [_canvas scrollPageBy:1]; }
- (void)keyHome { [_canvas scrollToEdge:NO]; }
- (void)keyEnd { [_canvas scrollToEdge:YES]; }

- (void)updateBattery {
    _canvas.batteryLevel = UIDevice.currentDevice.batteryLevel;
    _canvas.batteryState = UIDevice.currentDevice.batteryState;
}

- (void)keyboardFrameChanged:(NSNotification *)notification {
    CGRect frame = [notification.userInfo[UIKeyboardFrameEndUserInfoKey] CGRectValue];
    CGRect localFrame = [_canvas convertRect:frame fromView:nil];
    CGRect overlap = CGRectIntersection(_canvas.bounds, localFrame);
    _canvas.keyboardHeight = CGRectIsNull(overlap) ? 0 : overlap.size.height;
    [_canvas setNeedsDisplay];
}

- (void)keyboardHidden {
    _canvas.keyboardHeight = 0;
    [_canvas setNeedsDisplay];
}

- (void)startSampling {
    if (_sampleTimer || _canvas.isPaused) return;
    __weak typeof(self) weakSelf = self;
    _sampleTimer = [NSTimer scheduledTimerWithTimeInterval:1.0 repeats:YES block:^(NSTimer *timer) {
        [weakSelf collectOnce];
    }];
    [self collectOnce];
}

- (void)stopSampling {
    [_sampleTimer invalidate];
    _sampleTimer = nil;
}

- (void)appDidBackground {
    [self stopSampling];
    [_displayLink setPaused:YES];
}

- (void)appWillForeground {
    [_displayLink setPaused:NO];
    [self startSampling];
}

- (void)collectOnce {
    if (_sampling || _canvas.isPaused || UIApplication.sharedApplication.applicationState != UIApplicationStateActive) return;
    _sampling = YES;
    __weak typeof(self) weakSelf = self;
    ITopMonitor *monitor = _monitor;
    dispatch_async(_monitorQueue, ^{
        ITopSnapshot *snapshot = [monitor collect];
        dispatch_async(dispatch_get_main_queue(), ^{
            __strong typeof(weakSelf) self = weakSelf;
            if (!self) return;
            self->_sampling = NO;
            if (self->_canvas.isPaused || UIApplication.sharedApplication.applicationState != UIApplicationStateActive) return;
            self->_canvas.thermalState = snapshot.thermalState;
            self->_canvas.appAvailableMemory = snapshot.appAvailableMemory;
            self->_canvas.collectionMilliseconds = snapshot.collectionMilliseconds;
            self->_canvas.wifiReceiveBytesPerSecond = snapshot.wifiReceiveBytesPerSecond;
            self->_canvas.wifiSendBytesPerSecond = snapshot.wifiSendBytesPerSecond;
            self->_canvas.cellularReceiveBytesPerSecond = snapshot.cellularReceiveBytesPerSecond;
            self->_canvas.cellularSendBytesPerSecond = snapshot.cellularSendBytesPerSecond;
            self->_canvas.networkAvailable = snapshot.networkAvailable;
            self->_canvas.energyAvailable = snapshot.energyAvailable;
            self->_canvas.ownerEnergyPowerMilliwatts = 0;
            for (ITopProcess *process in snapshot.processes) {
                if ([process.name isEqualToString:@"itop"] && process.energyAvailable) {
                    self->_canvas.ownerEnergyPowerMilliwatts = process.energyPowerMilliwatts;
                    break;
                }
            }
            self->_canvas.snapshot = snapshot;
            [self updateBattery];
            if (!self->_canvas.selectedPID && snapshot.processes.count) self->_canvas.selectedPID = snapshot.processes.firstObject.pid;
        });
    });
}

- (void)onDisplayFrame:(CADisplayLink *)link {
    _fpsFrameCount++;
    if (_fpsWindowStart == 0) _fpsWindowStart = link.timestamp;
    CFTimeInterval elapsed = link.timestamp - _fpsWindowStart;
    if (elapsed >= 0.9) {
        double measured = _fpsFrameCount / MAX(0.001, elapsed);
        _fpsFrameCount = 0;
        _fpsWindowStart = link.timestamp;
        if (fabs(_canvas.displayFPS - measured) >= 1.0) {
            _canvas.displayFPS = measured;
            [_canvas setNeedsDisplay];
        }
    }
}

- (void)itopShowHelp {
    if (_canvas.isSearching) [_canvas cancelSearch];
    if (_canvas.isJumping) [_canvas cancelJump];
    _canvas.helpMode = !_canvas.helpMode;
    [_canvas setNeedsDisplay];
}

- (void)itopChangeSort {
    _canvas.helpMode = NO;
    NSArray *keys = @[@"CPU%", @"RES", @"PID", @"Command"];
    NSString *current = _canvas.sortKey ?: @"CPU%";
    NSUInteger index = [keys indexOfObject:current];
    NSString *next = keys[(index == NSNotFound ? 0 : (index + 1) % keys.count)];
    _canvas.sortKey = next;
    _canvas.sortDescending = [next isEqualToString:@"CPU%"] || [next isEqualToString:@"RES"];
    _canvas.scrollOffset = 0;
    [_canvas setNeedsDisplay];
}

- (void)itopChangeFilter {
    _canvas.helpMode = NO;
    [_canvas beginSearch];
}

- (void)itopBeginJump {
    _canvas.helpMode = NO;
    [_canvas beginJump];
}

- (void)itopTogglePause {
    _canvas.paused = !_canvas.isPaused;
    if (_canvas.isPaused) [self stopSampling];
    else [self startSampling];
    [_canvas setNeedsDisplay];
}

- (void)itopRefresh {
    if (_canvas.isPaused) {
        _canvas.paused = NO;
        [self startSampling];
    } else {
        [self collectOnce];
    }
}
@end

@interface ITopAppDelegate : UIResponder <UIApplicationDelegate>
@property (nonatomic, strong) UIWindow *window;
@end

@implementation ITopAppDelegate
- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)options {
    self.window = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
    self.window.rootViewController = [ITopViewController new];
    [self.window makeKeyAndVisible];
    return YES;
}
- (UIInterfaceOrientationMask)application:(UIApplication *)application supportedInterfaceOrientationsForWindow:(UIWindow *)window {
    return UIInterfaceOrientationMaskAll;
}
@end

// Self-test expectation helper: prints nothing on success, one terse line on
// failure, and returns 1 so callers can accumulate a failure count.
static int ITopExpect(BOOL condition, const char *what) {
    if (!condition) {
        printf("  FAIL: %s\n", what);
        return 1;
    }
    return 0;
}

static int ITopSelfTest(void) {
    @autoreleasepool {
        // Persistence hygiene: every canvas test runs against a throwaway
        // "itop-test" suite, never the standard user defaults, and the domain
        // is dropped in @finally even when a check returns early.
        [NSUserDefaults.standardUserDefaults removePersistentDomainForName:@"itop-test"];  // stale-domain guard
        NSUserDefaults *testSuite = [[NSUserDefaults alloc] initWithSuiteName:@"itop-test"];
        @try {
        ITopMonitor *monitor = [ITopMonitor new];
        ITopSnapshot *snapshot = [monitor collect];
        printf("itop smoke test\n");
        printf("  device: %s / iOS %s\n", snapshot.machine.UTF8String ?: "?", snapshot.osVersion.UTF8String ?: "?");
        printf("  cores: %lu, memory: %llu bytes\n", (unsigned long)snapshot.coreUsage.count,
               (unsigned long long)snapshot.memoryTotal);
        ITopProcTaskInfo ownTask = {0};
        int ownTaskBytes = proc_pidinfo(getpid(), ITopProcPIDTaskInfo, 0, &ownTask, (int)sizeof(ownTask));
        printf("  self PROC_PIDTASKINFO: %d bytes (ABI size %lu)\n", ownTaskBytes, (unsigned long)sizeof(ownTask));
        struct timespec pause = { .tv_sec = 0, .tv_nsec = 250000000 };
        nanosleep(&pause, NULL);
        ITopSnapshot *second = [monitor collect];
        NSUInteger validProcesses = 0;
        double peakProcessCPU = 0;
        for (ITopProcess *process in second.processes) {
            if (isfinite(process.cpu) && isfinite((double)process.residentBytes) && process.threads >= 0) {
                validProcesses++;
                peakProcessCPU = MAX(peakProcessCPU, process.cpu);
            }
        }
        double memoryPercent = second.memoryTotal ? (double)second.memoryUsed * 100.0 / (double)second.memoryTotal : 0;
        printf("  live metrics: CPU %.1f%%, RAM %.1f%%\n", second.cpu, memoryPercent);
        printf("  diagnostics: thermal %s, collect %.1fms, net %s, jetsam %llu bytes, energy %s\n",
               second.thermalState.UTF8String ?: "?", second.collectionMilliseconds,
               second.networkAvailable ? "interfaces" : "unavailable",
               (unsigned long long)second.appAvailableMemory,
               second.energyAvailable ? "counters" : "unavailable");
        printf("  process list: %lu PIDs, %lu readable; second sample %lu valid, peak %.1f%%\n",
               (unsigned long)second.processCount, (unsigned long)second.processes.count,
               (unsigned long)validProcesses, peakProcessCPU);
        if (!snapshot.machine.length || snapshot.memoryTotal == 0 || snapshot.coreUsage.count == 0 ||
            !snapshot.processAccess || snapshot.processCount == 0 || validProcesses == 0 ||
            !isfinite(second.cpu) || second.cpu < 0 || second.cpu > 100 ||
            second.memoryUsed > second.memoryTotal) {
            fprintf(stderr, "FAIL: required system/process statistics unavailable\n");
            return 1;
        }
        if (!second.thermalState.length || !isfinite(second.collectionMilliseconds)) {
            fprintf(stderr, "FAIL: diagnostic field sampling failed\n");
            return 1;
        }
        if (second.processes.count == 0) {
            printf("PASS: host metrics and process enumeration work; per-process metadata is restricted in this shell context\n");
        } else {
            printf("PASS: Mach CPU/memory, proc_info enumeration, and diagnostic sampling are available\n");
        }

        // ------------------------------------------------------------------
        // TASK A self-check: in-place filter editing (htop F4-style).
        // Verifies the shared search-tag geometry + tap classification at
        // narrow (~70), wide (~130) and degenerate column counts, then the
        // canvas-level edit round trip (tap-to-edit reopens with current text,
        // Return accepts, cancel preserves, ✕/clearFilter clears).
        // ------------------------------------------------------------------
        int failures = 0;
        printf("filter-tap self-check\n");
        NSInteger tagColumn;
        NSUInteger tagLength;
        ITopSearchTagGeometry(100, @"spring", 15, &tagColumn, &tagLength);
        // "search:" (7) + "spring" (6) + " ✕" (2) = 15 cells, right-aligned at columns-16.
        failures += ITopExpect(tagColumn == 84 && tagLength == 15, "geometry: search:spring at 100 cols");
        failures += ITopExpect(ITopSearchTapAction(YES, tagColumn, tagLength, 84) == ITopSearchTapEdit &&
                               ITopSearchTapAction(YES, tagColumn, tagLength, 90) == ITopSearchTapEdit,
                               "tap: interior cells route to edit");
        failures += ITopExpect(ITopSearchTapAction(YES, tagColumn, tagLength, 97) == ITopSearchTapClear &&
                               ITopSearchTapAction(YES, tagColumn, tagLength, 98) == ITopSearchTapClear,
                               "tap: last 2 cells are the ✕ clear zone");
        failures += ITopExpect(ITopSearchTapAction(YES, tagColumn, tagLength, 83) == ITopSearchTapNone &&
                               ITopSearchTapAction(YES, tagColumn, tagLength, 99) == ITopSearchTapNone,
                               "tap: cells outside the tag are inert");
        failures += ITopExpect(ITopSearchTapAction(NO, 93, 6, 93) == ITopSearchTapEdit,
                               "tap: idle search tag always edits (no ✕ exists)");
        ITopSearchTagGeometry(70, @"spring", 15, &tagColumn, &tagLength);
        failures += ITopExpect(tagColumn == 54 && tagLength == 15, "geometry: narrow ~70 col layout");
        ITopSearchTagGeometry(130, @"spring", 15, &tagColumn, &tagLength);
        failures += ITopExpect(tagColumn == 114 && tagLength == 15, "geometry: wide ~130 col layout");
        ITopSearchTagGeometry(24, [@"x" stringByPaddingToLength:40 withString:@"x" startingAtIndex:0],
                              5, &tagColumn, &tagLength);
        failures += ITopExpect(tagColumn == NSNotFound, "geometry: oversized filter hides the tag (no wrap/negative)");
        ITopSearchTagGeometry(70, @"", 64, &tagColumn, &tagLength);
        failures += ITopExpect(tagColumn == NSNotFound, "geometry: tag hidden when it would collide with the title");
        ITopCanvas *filterCanvas = [[ITopCanvas alloc] initWithFrame:CGRectMake(0, 0, 1024, 768)];
        filterCanvas.settingStore = testSuite;
        [filterCanvas loadStoredSettingsFromDefaults:testSuite];   // deterministic empty baseline
        [filterCanvas beginSearch];
        failures += ITopExpect(filterCanvas.isSearching, "edit: keyboard prompt opens from empty");
        [filterCanvas insertText:@"spring"];
        [filterCanvas insertText:@"\n"];
        failures += ITopExpect(!filterCanvas.isSearching && [filterCanvas.filterText isEqualToString:@"spring"],
                               "edit: Return accepts the typed filter");
        [filterCanvas beginSearch];          // tap to edit an existing filter (F4 flow)
        failures += ITopExpect(filterCanvas.isSearching, "edit: reopen with an active filter");
        [filterCanvas cancelSearch];
        failures += ITopExpect(!filterCanvas.isSearching && [filterCanvas.filterText isEqualToString:@"spring"],
                               "edit: Esc keeps the previously accepted filter");
        [filterCanvas clearFilter];          // the ✕ tap path
        failures += ITopExpect(!filterCanvas.isSearching && filterCanvas.filterText.length == 0,
                               "clear: ✕ clears in place without keyboard");
        if (failures) {
            fprintf(stderr, "FAIL: filter-tap self-check: %d assertion(s) failed\n", failures);
            return 1;
        }
        printf("  filter geometry + in-place edit flow: ok\n");

        // ------------------------------------------------------------------
        // TASK B self-check: paging and jump-by-PID math on fabricated rows.
        // ------------------------------------------------------------------
        printf("paging & jump self-check\n");
        NSMutableArray<ITopProcess *> *probe = [NSMutableArray array];
        for (NSUInteger i = 0; i < 50; i++) {
            ITopProcess *process = [ITopProcess new];
            process.pid = (pid_t)(1000 + i);
            process.name = @"probe";
            process.cpu = (double)i;
            [probe addObject:process];
        }
        failures = 0;
        // Page clamping: 50 rows in a 10-row viewport -> offset window [0,40].
        failures += ITopExpect(ITopScrollMaximum(50, 10) == 40, "page: max offset for 50 rows/10 viewport");
        failures += ITopExpect(ITopScrollClamp(100, 50, 10) == 40, "page: bottom overshoot clamps to 40");
        failures += ITopExpect(ITopScrollClamp(0, 10, 10) == 0, "page: list shorter than viewport never scrolls");
        failures += ITopExpect(ITopPagedOffset(20, 1, 50, 10) == 30, "page: PgDn advances one page");
        failures += ITopExpect(ITopPagedOffset(20, -1, 50, 10) == 10, "page: PgUp backs up one page");
        failures += ITopExpect(ITopPagedOffset(5, -1, 50, 10) == 0, "page: PgUp stops at top (no underflow)");
        failures += ITopExpect(ITopPagedOffset(35, 1, 50, 10) == 40, "page: PgDn stops at bottom");
        failures += ITopExpect(ITopPagedOffset(7, 1, 50, 0) == 7, "page: zero-capacity before first layout is inert");
        failures += ITopExpect(ITopScrollMaximum(0, 0) == 0, "page: empty list stays at offset 0");
        // Jump lookup over the fabricated visible rows (PIDs 1000..1049).
        failures += ITopExpect(ITopRowIndexOfPID(probe, 1023) == 23, "jump: existing PID found at its index");
        failures += ITopExpect(ITopRowIndexOfPID(probe, 7) == NSNotFound, "jump: absent PID reports not-found");
        failures += ITopExpect(ITopJumpTargetPID(@"312") == 312, "jump: digits parse to a PID");
        failures += ITopExpect(ITopJumpTargetPID(@"") == 0, "jump: empty draft commits to nothing");
        failures += ITopExpect(ITopJumpTargetPID(@"-5") == 0, "jump: negative/invalid draft rejected");
        failures += ITopExpect(ITopJumpTargetPID(@"99999999999999") == 0, "jump: overflow draft rejected");
        // End-to-end prompt cycle: begin, type, commit to a real PID, then miss.
        ITopCanvas *jumpCanvas = [[ITopCanvas alloc] initWithFrame:CGRectMake(0, 0, 1024, 768)];
        jumpCanvas.settingStore = testSuite;
        [jumpCanvas loadStoredSettingsFromDefaults:testSuite];
        ITopSnapshot *probeSnapshot = [ITopSnapshot new];
        probeSnapshot.processes = probe;
        probeSnapshot.processCount = probe.count;
        jumpCanvas.snapshot = probeSnapshot;
        [jumpCanvas beginJump];
        failures += ITopExpect(jumpCanvas.isJumping && !jumpCanvas.isSearching, "jump: J opens the prompt");
        [jumpCanvas insertText:@"1023"];
        [jumpCanvas insertText:@"\n"];
        failures += ITopExpect(jumpCanvas.selectedPID == 1023 && !jumpCanvas.isJumping, "jump: Return selects and closes");
        [jumpCanvas beginJump];
        [jumpCanvas insertText:@"7"];
        [jumpCanvas insertText:@"\n"];
        failures += ITopExpect(jumpCanvas.isJumping && jumpCanvas.selectedPID == 1023, "jump: miss keeps prompt and selection");
        [jumpCanvas cancelJump];
        failures += ITopExpect(!jumpCanvas.isJumping && jumpCanvas.isSearching == NO, "jump: Esc closes the prompt");
        if (failures) {
            fprintf(stderr, "FAIL: paging & jump self-check: %d assertion(s) failed\n", failures);
            return 1;
        }
        printf("  paging clamps and jump-to-PID: ok\n");

        // ------------------------------------------------------------------
        // TASK C self-check: persisted settings round trip with validation.
        // A known configuration is written into the throwaway "itop-test"
        // suite, loaded back through the canvas, then garbage is fed in and
        // the defaults must take over. The @finally at function scope drops
        // the suite domain after this block (and on any early return).
        // ------------------------------------------------------------------
        printf("settings persistence self-check\n");
        failures = 0;
        [testSuite setObject:@"Command" forKey:ITopDefaultsSortKey];
        [testSuite setBool:YES forKey:ITopDefaultsSortDesc];       // non-default: Command usually sorts ascending
        [testSuite setObject:@"launchd" forKey:ITopDefaultsFilter];
        ITopCanvas *loadedCanvas = [[ITopCanvas alloc] initWithFrame:CGRectMake(0, 0, 800, 600)];
        loadedCanvas.settingStore = testSuite;
        [loadedCanvas loadStoredSettingsFromDefaults:testSuite];
        failures += ITopExpect([loadedCanvas.sortKey isEqualToString:@"Command"], "settings: sort key round trip");
        failures += ITopExpect(loadedCanvas.sortDescending == YES, "settings: stored direction beats per-key default");
        failures += ITopExpect([loadedCanvas.filterText isEqualToString:@"launchd"], "settings: filter round trip");
        failures += ITopExpect([[testSuite stringForKey:ITopDefaultsSortKey] isEqualToString:@"Command"],
                               "settings: loading does not rewrite the store");
        [testSuite setObject:@"EVIL" forKey:ITopDefaultsSortKey];
        [testSuite setObject:@[ @"garbage" ] forKey:ITopDefaultsSortDesc];
        [testSuite setObject:@[ @"garbage" ] forKey:ITopDefaultsFilter];
        ITopCanvas *garbageCanvas = [[ITopCanvas alloc] initWithFrame:CGRectMake(0, 0, 800, 600)];
        garbageCanvas.settingStore = testSuite;
        [garbageCanvas loadStoredSettingsFromDefaults:testSuite];
        failures += ITopExpect([garbageCanvas.sortKey isEqualToString:@"CPU%"] && garbageCanvas.sortDescending == YES,
                               "settings: unknown sort key degrades to CPU% descending");
        failures += ITopExpect(garbageCanvas.filterText.length == 0, "settings: non-string filter degrades to empty");
        garbageCanvas.sortKey = @"PID";                            // change-driven writes
        failures += ITopExpect([[testSuite stringForKey:ITopDefaultsSortKey] isEqualToString:@"PID"],
                               "settings: sort change is persisted on change");
        garbageCanvas.sortDescending = NO;
        failures += ITopExpect([testSuite boolForKey:ITopDefaultsSortDesc] == NO,
                               "settings: direction change is persisted on change");
        if (failures) {
            fprintf(stderr, "FAIL: settings persistence self-check: %d assertion(s) failed\n", failures);
            return 1;
        }
        printf("  persistent sort/filter + validation: ok\n");
        return 0;
        } @finally {
            [testSuite removePersistentDomainForName:@"itop-test"];
        }
    }
}

int main(int argc, char *argv[]) {
    @autoreleasepool {
        if (argc > 1 && strcmp(argv[1], "--self-test") == 0) return ITopSelfTest();
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([ITopAppDelegate class]));
    }
}
