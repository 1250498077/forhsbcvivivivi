import java.io.*;
import java.util.*;
import java.util.concurrent.*;
import java.util.function.Consumer;

/**
 * 增强版命令执行器
 * 支持实时输出、异步执行、重试机制等高级特性
 */
public class AdvancedCommandExecutor extends CommandExecutor {
    
    private boolean printRealTimeOutput = false;
    private int maxRetries = 0;
    private long retryDelayMs = 1000;
    private Consumer<String> outputConsumer;
    
    /**
     * 启用实时输出打印
     */
    public AdvancedCommandExecutor withRealTimeOutput() {
        this.printRealTimeOutput = true;
        return this;
    }
    
    /**
     * 设置输出消费者
     */
    public AdvancedCommandExecutor withOutputConsumer(Consumer<String> consumer) {
        this.outputConsumer = consumer;
        return this;
    }
    
    /**
     * 设置重试次数
     */
    public AdvancedCommandExecutor withRetry(int maxRetries, long retryDelayMs) {
        this.maxRetries = maxRetries;
        this.retryDelayMs = retryDelayMs;
        return this;
    }
    
    /**
     * 执行命令（支持重试）
     */
    @Override
    public CommandResult execute(List<String> command) throws CommandExecutionException {
        CommandExecutionException lastException = null;
        
        for (int attempt = 0; attempt <= maxRetries; attempt++) {
            try {
                if (attempt > 0) {
                    System.out.println("重试第 " + attempt + " 次...");
                    Thread.sleep(retryDelayMs);
                }
                
                return executeWithRealTimeOutput(command);
                
            } catch (CommandExecutionException e) {
                lastException = e;
                if (attempt == maxRetries) {
                    throw e;
                }
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                throw new CommandExecutionException("命令执行被中断", e);
            }
        }
        
        throw lastException;
    }
    
    /**
     * 执行命令并支持实时输出
     */
    private CommandResult executeWithRealTimeOutput(List<String> command) 
            throws CommandExecutionException {
        
        List<String> fullCommand = buildFullCommand(command);
        
        try {
            ProcessBuilder pb = new ProcessBuilder(fullCommand);
            pb.redirectErrorStream(true);
            
            Process process = pb.start();
            
            StringBuilder output = new StringBuilder();
            
            // 实时读取输出
            CompletableFuture<Void> outputFuture = CompletableFuture.runAsync(() -> {
                try (BufferedReader reader = new BufferedReader(
                        new InputStreamReader(process.getInputStream()))) {
                    String line;
                    while ((line = reader.readLine()) != null) {
                        output.append(line).append("\n");
                        
                        // 实时输出
                        if (printRealTimeOutput) {
                            System.out.println(line);
                        }
                        
                        // 输出消费者
                        if (outputConsumer != null) {
                            outputConsumer.accept(line);
                        }
                    }
                } catch (IOException e) {
                    throw new UncheckedIOException(e);
                }
            });
            
            boolean finished = process.waitFor(300, TimeUnit.SECONDS);
            
            if (!finished) {
                process.destroyForcibly();
                throw new CommandExecutionException("命令执行超时");
            }
            
            outputFuture.get(5, TimeUnit.SECONDS);
            
            int exitCode = process.exitValue();
            
            return new CommandResult(
                exitCode,
                output.toString().trim(),
                "",
                exitCode == 0
            );
            
        } catch (IOException | InterruptedException | ExecutionException | TimeoutException e) {
            throw new CommandExecutionException("命令执行失败: " + e.getMessage(), e);
        }
    }
    
    /**
     * 异步执行命令
     */
    public CompletableFuture<CommandResult> executeAsync(String... command) {
        return CompletableFuture.supplyAsync(() -> {
            try {
                return execute(command);
            } catch (CommandExecutionException e) {
                throw new CompletionException(e);
            }
        });
    }
    
    /**
     * 并行执行多个命令
     */
    public List<CommandResult> executeParallel(List<String[]> commands) 
            throws CommandExecutionException {
        
        List<CompletableFuture<CommandResult>> futures = new ArrayList<>();
        
        for (String[] command : commands) {
            futures.add(executeAsync(command));
        }
        
        try {
            CompletableFuture.allOf(futures.toArray(new CompletableFuture[0])).join();
            
            List<CommandResult> results = new ArrayList<>();
            for (CompletableFuture<CommandResult> future : futures) {
                results.add(future.get());
            }
            
            return results;
            
        } catch (Exception e) {
            throw new CommandExecutionException("并行执行失败", e);
        }
    }
}

/**
 * 命令执行上下文 - 用于在命令链中传递状态
 */
class CommandContext {
    private final Map<String, Object> data = new HashMap<>();
    private final List<CommandExecutor.CommandResult> results = new ArrayList<>();
    
    public void put(String key, Object value) {
        data.put(key, value);
    }
    
    public Object get(String key) {
        return data.get(key);
    }
    
    public String getString(String key) {
        Object value = data.get(key);
        return value != null ? value.toString() : null;
    }
    
    public void addResult(CommandExecutor.CommandResult result) {
        results.add(result);
    }
    
    public List<CommandExecutor.CommandResult> getResults() {
        return Collections.unmodifiableList(results);
    }
    
    public CommandExecutor.CommandResult getLastResult() {
        return results.isEmpty() ? null : results.get(results.size() - 1);
    }
    
    public CommandExecutor.CommandResult getResult(int index) {
        return index >= 0 && index < results.size() ? results.get(index) : null;
    }
}

/**
 * 带上下文的命令链执行器
 */
class ContextualCommandExecutor {
    
    private final CommandExecutor executor;
    private final CommandContext context;
    
    public ContextualCommandExecutor(CommandExecutor executor) {
        this.executor = executor;
        this.context = new CommandContext();
    }
    
    /**
     * 执行命令链并维护上下文
     */
    public CommandContext executeChain(List<ContextualCommandCallback> callbacks) 
            throws CommandExecutor.CommandExecutionException {
        
        for (ContextualCommandCallback callback : callbacks) {
            // 构造命令
            String[] command = callback.buildCommand(context);
            
            if (command == null || command.length == 0) {
                break; // 结束链
            }
            
            // 执行命令
            CommandExecutor.CommandResult result = executor.execute(command);
            context.addResult(result);
            
            // 后处理
            callback.afterExecution(context, result);
            
            // 如果失败且需要停止
            if (!result.isSuccess() && callback.stopOnFailure()) {
                break;
            }
        }
        
        return context;
    }
    
    /**
     * 带上下文的命令回调接口
     */
    public interface ContextualCommandCallback {
        /**
         * 根据上下文构造命令
         */
        String[] buildCommand(CommandContext context);
        
        /**
         * 命令执行后的处理
         */
        default void afterExecution(CommandContext context, CommandExecutor.CommandResult result) {
            // 默认不做处理
        }
        
        /**
         * 是否在失败时停止
         */
        default boolean stopOnFailure() {
            return true;
        }
    }
}

/**
 * 使用示例
 */
class AdvancedUsageExample {
    
    public static void main(String[] args) {
        // 示例1: 实时输出
        realTimeOutputExample();
        
        // 示例2: 带重试的执行
        retryExample();
        
        // 示例3: 异步执行
        asyncExample();
        
        // 示例4: 带上下文的命令链
        contextualChainExample();
        
        // 示例5: 并行执行
        parallelExample();
    }
    
    /**
     * 示例1: 实时输出
     */
    public static void realTimeOutputExample() {
        System.out.println("=== 实时输出示例 ===");
        try {
            AdvancedCommandExecutor executor = new AdvancedCommandExecutor()
                    .withRealTimeOutput();
            
            executor.execute("echo", "开始执行...");
            executor.execute("echo", "第二步...");
            executor.execute("echo", "完成!");
            
        } catch (CommandExecutor.CommandExecutionException e) {
            e.printStackTrace();
        }
        System.out.println();
    }
    
    /**
     * 示例2: 带重试的执行
     */
    public static void retryExample() {
        System.out.println("=== 重试示例 ===");
        try {
            AdvancedCommandExecutor executor = new AdvancedCommandExecutor()
                    .withRetry(3, 1000); // 最多重试3次,每次间隔1秒
            
            // 模拟可能失败的命令
            executor.execute("ping", "-c", "1", "unreachable.example.com");
            
        } catch (CommandExecutor.CommandExecutionException e) {
            System.out.println("命令最终失败: " + e.getMessage());
        }
        System.out.println();
    }
    
    /**
     * 示例3: 异步执行
     */
    public static void asyncExample() {
        System.out.println("=== 异步执行示例 ===");
        try {
            AdvancedCommandExecutor executor = new AdvancedCommandExecutor();
            
            CompletableFuture<CommandExecutor.CommandResult> future1 = 
                executor.executeAsync("echo", "任务1");
            
            CompletableFuture<CommandExecutor.CommandResult> future2 = 
                executor.executeAsync("echo", "任务2");
            
            // 等待所有任务完成
            CompletableFuture.allOf(future1, future2).join();
            
            System.out.println("任务1结果: " + future1.get().getOutput());
            System.out.println("任务2结果: " + future2.get().getOutput());
            
        } catch (Exception e) {
            e.printStackTrace();
        }
        System.out.println();
    }
    
    /**
     * 示例4: 带上下文的命令链
     */
    public static void contextualChainExample() {
        System.out.println("=== 带上下文的命令链示例 ===");
        try {
            CommandExecutor executor = new CommandExecutor();
            ContextualCommandExecutor contextExecutor = 
                new ContextualCommandExecutor(executor);
            
            List<ContextualCommandExecutor.ContextualCommandCallback> callbacks = 
                new ArrayList<>();
            
            // 步骤1: 创建临时目录
            callbacks.add(new ContextualCommandExecutor.ContextualCommandCallback() {
                @Override
                public String[] buildCommand(CommandContext context) {
                    return new String[]{"mktemp", "-d"};
                }
                
                @Override
                public void afterExecution(CommandContext context, 
                                         CommandExecutor.CommandResult result) {
                    if (result.isSuccess()) {
                        // 保存临时目录路径到上下文
                        context.put("tmpDir", result.getOutput().trim());
                        System.out.println("临时目录: " + context.getString("tmpDir"));
                    }
                }
            });
            
            // 步骤2: 在临时目录中创建文件
            callbacks.add(new ContextualCommandExecutor.ContextualCommandCallback() {
                @Override
                public String[] buildCommand(CommandContext context) {
                    String tmpDir = context.getString("tmpDir");
                    if (tmpDir != null) {
                        return new String[]{"touch", tmpDir + "/test.txt"};
                    }
                    return null;
                }
                
                @Override
                public void afterExecution(CommandContext context, 
                                         CommandExecutor.CommandResult result) {
                    System.out.println("文件创建: " + 
                        (result.isSuccess() ? "成功" : "失败"));
                }
            });
            
            // 步骤3: 列出文件
            callbacks.add(context -> {
                String tmpDir = context.getString("tmpDir");
                if (tmpDir != null) {
                    return new String[]{"ls", "-la", tmpDir};
                }
                return null;
            });
            
            CommandContext result = contextExecutor.executeChain(callbacks);
            System.out.println("执行了 " + result.getResults().size() + " 个命令");
            
        } catch (CommandExecutor.CommandExecutionException e) {
            e.printStackTrace();
        }
        System.out.println();
    }
    
    /**
     * 示例5: 并行执行
     */
    public static void parallelExample() {
        System.out.println("=== 并行执行示例 ===");
        try {
            AdvancedCommandExecutor executor = new AdvancedCommandExecutor();
            
            List<String[]> commands = Arrays.asList(
                new String[]{"echo", "任务A"},
                new String[]{"echo", "任务B"},
                new String[]{"echo", "任务C"}
            );
            
            long start = System.currentTimeMillis();
            List<CommandExecutor.CommandResult> results = executor.executeParallel(commands);
            long duration = System.currentTimeMillis() - start;
            
            System.out.println("并行执行 " + results.size() + " 个任务");
            System.out.println("耗时: " + duration + "ms");
            
            for (int i = 0; i < results.size(); i++) {
                System.out.println("任务 " + (i + 1) + ": " + 
                    results.get(i).getOutput());
            }
            
        } catch (CommandExecutor.CommandExecutionException e) {
            e.printStackTrace();
        }
        System.out.println();
    }
}
