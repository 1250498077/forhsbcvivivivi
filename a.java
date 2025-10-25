import java.io.*;
import java.util.*;
import java.util.concurrent.*;
import java.util.function.Function;

/**
 * 命令执行器 - 封装 ProcessBuilder，支持 Docker 和 Windows
 * 支持命令链式执行和回调处理
 */
public class CommandExecutor {
    
    // 操作系统类型
    private final OsType osType;
    
    // 公共前置命令
    private final List<String> commonPrefixCommands;
    
    // 工作目录
    private File workingDirectory;
    
    // 环境变量
    private Map<String, String> environment;
    
    // 超时时间（秒）
    private long timeoutSeconds = 300;
    
    // 是否在 Docker 中运行
    private boolean isDocker = false;
    
    // Docker 容器名称（如果在 Docker 中运行）
    private String dockerContainer;
    
    public CommandExecutor() {
        this.osType = detectOsType();
        this.commonPrefixCommands = new ArrayList<>();
        this.environment = new HashMap<>();
    }
    
    /**
     * 检测操作系统类型
     */
    private OsType detectOsType() {
        String os = System.getProperty("os.name").toLowerCase();
        if (os.contains("win")) {
            return OsType.WINDOWS;
        } else if (os.contains("nix") || os.contains("nux") || os.contains("mac")) {
            return OsType.UNIX;
        }
        return OsType.UNIX;
    }
    
    /**
     * 设置公共前置命令
     */
    public CommandExecutor withPrefixCommands(String... commands) {
        this.commonPrefixCommands.clear();
        this.commonPrefixCommands.addAll(Arrays.asList(commands));
        return this;
    }
    
    /**
     * 添加公共前置命令
     */
    public CommandExecutor addPrefixCommand(String command) {
        this.commonPrefixCommands.add(command);
        return this;
    }
    
    /**
     * 设置工作目录
     */
    public CommandExecutor workingDirectory(String path) {
        this.workingDirectory = new File(path);
        return this;
    }
    
    /**
     * 设置环境变量
     */
    public CommandExecutor withEnvironment(String key, String value) {
        this.environment.put(key, value);
        return this;
    }
    
    /**
     * 批量设置环境变量
     */
    public CommandExecutor withEnvironment(Map<String, String> env) {
        this.environment.putAll(env);
        return this;
    }
    
    /**
     * 设置超时时间
     */
    public CommandExecutor timeout(long seconds) {
        this.timeoutSeconds = seconds;
        return this;
    }
    
    /**
     * 设置 Docker 执行环境
     */
    public CommandExecutor inDocker(String containerName) {
        this.isDocker = true;
        this.dockerContainer = containerName;
        return this;
    }
    
    /**
     * 执行单个命令
     */
    public CommandResult execute(String... command) throws CommandExecutionException {
        return execute(Arrays.asList(command));
    }
    
    /**
     * 执行单个命令
     */
    public CommandResult execute(List<String> command) throws CommandExecutionException {
        List<String> fullCommand = buildFullCommand(command);
        
        try {
            ProcessBuilder pb = new ProcessBuilder(fullCommand);
            
            // 设置工作目录
            if (workingDirectory != null) {
                pb.directory(workingDirectory);
            }
            
            // 设置环境变量
            if (!environment.isEmpty()) {
                pb.environment().putAll(environment);
            }
            
            // 合并标准输出和错误输出
            pb.redirectErrorStream(true);
            
            Process process = pb.start();
            
            // 读取输出
            StringBuilder output = new StringBuilder();
            StringBuilder error = new StringBuilder();
            
            CompletableFuture<Void> outputFuture = CompletableFuture.runAsync(() -> {
                try (BufferedReader reader = new BufferedReader(
                        new InputStreamReader(process.getInputStream()))) {
                    String line;
                    while ((line = reader.readLine()) != null) {
                        output.append(line).append("\n");
                    }
                } catch (IOException e) {
                    throw new UncheckedIOException(e);
                }
            });
            
            // 等待进程完成
            boolean finished = process.waitFor(timeoutSeconds, TimeUnit.SECONDS);
            
            if (!finished) {
                process.destroyForcibly();
                throw new CommandExecutionException("命令执行超时: " + String.join(" ", command));
            }
            
            // 等待输出读取完成
            outputFuture.get(5, TimeUnit.SECONDS);
            
            int exitCode = process.exitValue();
            
            return new CommandResult(
                exitCode,
                output.toString().trim(),
                error.toString().trim(),
                exitCode == 0
            );
            
        } catch (IOException | InterruptedException | ExecutionException | TimeoutException e) {
            throw new CommandExecutionException("命令执行失败: " + e.getMessage(), e);
        }
    }
    
    /**
     * 执行命令链 - 使用回调函数构造下一个命令
     */
    public CommandResult executeChain(CommandChainBuilder builder) throws CommandExecutionException {
        CommandResult lastResult = null;
        String nextCommand = builder.getFirstCommand();
        
        while (nextCommand != null) {
            // 执行命令
            lastResult = execute(parseCommand(nextCommand));
            
            // 如果执行失败且需要中断
            if (!lastResult.isSuccess() && builder.shouldStopOnFailure()) {
                break;
            }
            
            // 通过回调获取下一个命令
            nextCommand = builder.getNextCommand(lastResult);
        }
        
        return lastResult;
    }
    
    /**
     * 执行命令链 - 简化版本
     */
    public List<CommandResult> executeChain(List<CommandCallback> callbacks) 
            throws CommandExecutionException {
        List<CommandResult> results = new ArrayList<>();
        CommandResult previousResult = null;
        
        for (CommandCallback callback : callbacks) {
            // 构造命令
            String[] command = callback.buildCommand(previousResult);
            
            if (command == null || command.length == 0) {
                break; // 结束链
            }
            
            // 执行命令
            CommandResult result = execute(command);
            results.add(result);
            
            // 如果失败且需要停止
            if (!result.isSuccess() && callback.stopOnFailure()) {
                break;
            }
            
            previousResult = result;
        }
        
        return results;
    }
    
    /**
     * 构建完整命令（包含前置命令和 Docker 包装）
     */
    private List<String> buildFullCommand(List<String> command) {
        List<String> fullCommand = new ArrayList<>();
        
        if (isDocker) {
            // Docker 执行命令
            fullCommand.add("docker");
            fullCommand.add("exec");
            fullCommand.add(dockerContainer);
            
            if (osType == OsType.UNIX) {
                fullCommand.add("sh");
                fullCommand.add("-c");
                // 构建完整的 shell 命令
                String shellCommand = buildShellCommand(command);
                fullCommand.add(shellCommand);
            } else {
                // Windows in Docker (通常也是 Linux 容器)
                fullCommand.add("sh");
                fullCommand.add("-c");
                String shellCommand = buildShellCommand(command);
                fullCommand.add(shellCommand);
            }
        } else {
            // 本地执行
            if (osType == OsType.WINDOWS) {
                fullCommand.add("cmd.exe");
                fullCommand.add("/c");
                // 构建 Windows 命令
                String windowsCommand = buildWindowsCommand(command);
                fullCommand.add(windowsCommand);
            } else {
                fullCommand.add("sh");
                fullCommand.add("-c");
                // 构建 Shell 命令
                String shellCommand = buildShellCommand(command);
                fullCommand.add(shellCommand);
            }
        }
        
        return fullCommand;
    }
    
    /**
     * 构建 Shell 命令（Unix/Linux）
     */
    private String buildShellCommand(List<String> command) {
        List<String> allCommands = new ArrayList<>();
        allCommands.addAll(commonPrefixCommands);
        allCommands.addAll(command);
        return String.join(" && ", allCommands);
    }
    
    /**
     * 构建 Windows 命令
     */
    private String buildWindowsCommand(List<String> command) {
        List<String> allCommands = new ArrayList<>();
        allCommands.addAll(commonPrefixCommands);
        allCommands.addAll(command);
        return String.join(" && ", allCommands);
    }
    
    /**
     * 解析命令字符串为数组
     */
    private String[] parseCommand(String command) {
        // 简单的空格分割，实际使用中可能需要更复杂的解析
        return command.split("\\s+");
    }
    
    /**
     * 操作系统类型枚举
     */
    enum OsType {
        WINDOWS, UNIX
    }
    
    /**
     * 命令执行结果
     */
    public static class CommandResult {
        private final int exitCode;
        private final String output;
        private final String error;
        private final boolean success;
        
        public CommandResult(int exitCode, String output, String error, boolean success) {
            this.exitCode = exitCode;
            this.output = output;
            this.error = error;
            this.success = success;
        }
        
        public int getExitCode() {
            return exitCode;
        }
        
        public String getOutput() {
            return output;
        }
        
        public String getError() {
            return error;
        }
        
        public boolean isSuccess() {
            return success;
        }
        
        @Override
        public String toString() {
            return "CommandResult{" +
                    "exitCode=" + exitCode +
                    ", success=" + success +
                    ", output='" + output + '\'' +
                    ", error='" + error + '\'' +
                    '}';
        }
    }
    
    /**
     * 命令回调接口
     */
    @FunctionalInterface
    public interface CommandCallback {
        /**
         * 根据上一个命令的结果构造下一个命令
         * @param previousResult 上一个命令的结果，第一个命令时为 null
         * @return 要执行的命令数组，返回 null 表示结束链
         */
        String[] buildCommand(CommandResult previousResult);
        
        /**
         * 是否在失败时停止
         */
        default boolean stopOnFailure() {
            return true;
        }
    }
    
    /**
     * 命令链构造器
     */
    public static class CommandChainBuilder {
        private String firstCommand;
        private Function<CommandResult, String> nextCommandFunction;
        private boolean stopOnFailure = true;
        
        public CommandChainBuilder firstCommand(String command) {
            this.firstCommand = command;
            return this;
        }
        
        public CommandChainBuilder then(Function<CommandResult, String> nextCommandFunction) {
            this.nextCommandFunction = nextCommandFunction;
            return this;
        }
        
        public CommandChainBuilder continueOnFailure() {
            this.stopOnFailure = false;
            return this;
        }
        
        public String getFirstCommand() {
            return firstCommand;
        }
        
        public String getNextCommand(CommandResult result) {
            return nextCommandFunction != null ? nextCommandFunction.apply(result) : null;
        }
        
        public boolean shouldStopOnFailure() {
            return stopOnFailure;
        }
    }
    
    /**
     * 命令执行异常
     */
    public static class CommandExecutionException extends Exception {
        public CommandExecutionException(String message) {
            super(message);
        }
        
        public CommandExecutionException(String message, Throwable cause) {
            super(message, cause);
        }
    }
}
