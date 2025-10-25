import java.io.*;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.concurrent.*;
import java.util.function.Function;

/**
 * 支持自定义编码的命令执行器
 * 解决 Windows 下中文乱码问题
 */
public class CommandExecutorWithEncoding {
    
    private final OsType osType;
    private final List<String> commonPrefixCommands;
    private File workingDirectory;
    private Map<String, String> environment;
    private long timeoutSeconds = 300;
    private boolean isDocker = false;
    private String dockerContainer;
    
    // 新增：输出流编码设置
    private Charset outputCharset;
    
    public CommandExecutorWithEncoding() {
        this.osType = detectOsType();
        this.commonPrefixCommands = new ArrayList<>();
        this.environment = new HashMap<>();
        
        // 根据操作系统自动设置默认编码
        if (osType == OsType.WINDOWS) {
            // Windows 默认使用 GBK
            this.outputCharset = Charset.forName("GBK");
        } else {
            // Unix/Linux 默认使用 UTF-8
            this.outputCharset = StandardCharsets.UTF_8;
        }
    }
    
    /**
     * 设置输出编码
     * 常用编码:
     * - GBK: 中文 Windows
     * - UTF-8: Linux/Mac
     * - GB2312: 简体中文
     */
    public CommandExecutorWithEncoding withOutputEncoding(String charsetName) {
        this.outputCharset = Charset.forName(charsetName);
        return this;
    }
    
    /**
     * 设置输出编码
     */
    public CommandExecutorWithEncoding withOutputEncoding(Charset charset) {
        this.outputCharset = charset;
        return this;
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
    
    public CommandExecutorWithEncoding withPrefixCommands(String... commands) {
        this.commonPrefixCommands.clear();
        this.commonPrefixCommands.addAll(Arrays.asList(commands));
        return this;
    }
    
    public CommandExecutorWithEncoding addPrefixCommand(String command) {
        this.commonPrefixCommands.add(command);
        return this;
    }
    
    public CommandExecutorWithEncoding workingDirectory(String path) {
        this.workingDirectory = new File(path);
        return this;
    }
    
    public CommandExecutorWithEncoding withEnvironment(String key, String value) {
        this.environment.put(key, value);
        return this;
    }
    
    public CommandExecutorWithEncoding withEnvironment(Map<String, String> env) {
        this.environment.putAll(env);
        return this;
    }
    
    public CommandExecutorWithEncoding timeout(long seconds) {
        this.timeoutSeconds = seconds;
        return this;
    }
    
    public CommandExecutorWithEncoding inDocker(String containerName) {
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
     * 执行单个命令 - 核心方法，使用指定编码读取输出
     */
    public CommandResult execute(List<String> command) throws CommandExecutionException {
        List<String> fullCommand = buildFullCommand(command);
        
        try {
            ProcessBuilder pb = new ProcessBuilder(fullCommand);
            
            if (workingDirectory != null) {
                pb.directory(workingDirectory);
            }
            
            if (!environment.isEmpty()) {
                pb.environment().putAll(environment);
            }
            
            // 不合并错误流，分别读取
            pb.redirectErrorStream(false);
            
            Process process = pb.start();
            
            // 使用指定编码读取输出
            StringBuilder output = new StringBuilder();
            StringBuilder error = new StringBuilder();
            
            // 读取标准输出（使用指定编码）
            CompletableFuture<Void> outputFuture = CompletableFuture.runAsync(() -> {
                try (BufferedReader reader = new BufferedReader(
                        new InputStreamReader(process.getInputStream(), outputCharset))) {
                    String line;
                    while ((line = reader.readLine()) != null) {
                        output.append(line).append("\n");
                    }
                } catch (IOException e) {
                    throw new UncheckedIOException(e);
                }
            });
            
            // 读取错误输出（使用指定编码）
            CompletableFuture<Void> errorFuture = CompletableFuture.runAsync(() -> {
                try (BufferedReader reader = new BufferedReader(
                        new InputStreamReader(process.getErrorStream(), outputCharset))) {
                    String line;
                    while ((line = reader.readLine()) != null) {
                        error.append(line).append("\n");
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
            errorFuture.get(5, TimeUnit.SECONDS);
            
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
     * 执行命令链
     */
    public List<CommandResult> executeChain(List<CommandCallback> callbacks) 
            throws CommandExecutionException {
        List<CommandResult> results = new ArrayList<>();
        CommandResult previousResult = null;
        
        for (CommandCallback callback : callbacks) {
            String[] command = callback.buildCommand(previousResult);
            
            if (command == null || command.length == 0) {
                break;
            }
            
            CommandResult result = execute(command);
            results.add(result);
            
            if (!result.isSuccess() && callback.stopOnFailure()) {
                break;
            }
            
            previousResult = result;
        }
        
        return results;
    }
    
    /**
     * 构建完整命令
     */
    private List<String> buildFullCommand(List<String> command) {
        List<String> fullCommand = new ArrayList<>();
        
        if (isDocker) {
            fullCommand.add("docker");
            fullCommand.add("exec");
            fullCommand.add(dockerContainer);
            fullCommand.add("sh");
            fullCommand.add("-c");
            String shellCommand = buildShellCommand(command);
            fullCommand.add(shellCommand);
        } else {
            if (osType == OsType.WINDOWS) {
                // Windows 使用 chcp 65001 设置 UTF-8 可能会有问题
                // 直接使用 cmd.exe /c
                fullCommand.add("cmd.exe");
                fullCommand.add("/c");
                String windowsCommand = buildWindowsCommand(command);
                fullCommand.add(windowsCommand);
            } else {
                fullCommand.add("sh");
                fullCommand.add("-c");
                String shellCommand = buildShellCommand(command);
                fullCommand.add(shellCommand);
            }
        }
        
        return fullCommand;
    }
    
    private String buildShellCommand(List<String> command) {
        List<String> allCommands = new ArrayList<>();
        allCommands.addAll(commonPrefixCommands);
        allCommands.addAll(command);
        return String.join(" && ", allCommands);
    }
    
    private String buildWindowsCommand(List<String> command) {
        List<String> allCommands = new ArrayList<>();
        allCommands.addAll(commonPrefixCommands);
        allCommands.addAll(command);
        return String.join(" && ", allCommands);
    }
    
    enum OsType {
        WINDOWS, UNIX
    }
    
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
        
        public int getExitCode() { return exitCode; }
        public String getOutput() { return output; }
        public String getError() { return error; }
        public boolean isSuccess() { return success; }
        
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
    
    @FunctionalInterface
    public interface CommandCallback {
        String[] buildCommand(CommandResult previousResult);
        
        default boolean stopOnFailure() {
            return true;
        }
    }
    
    public static class CommandExecutionException extends Exception {
        public CommandExecutionException(String message) {
            super(message);
        }
        
        public CommandExecutionException(String message, Throwable cause) {
            super(message, cause);
        }
    }
}
