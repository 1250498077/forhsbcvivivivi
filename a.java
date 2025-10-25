import java.io.*;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.concurrent.*;

/**
 * 支持直接输入字符串命令的执行器
 * 不需要手动拆分命令，直接写完整的命令字符串
 */
public class StringCommandExecutor {
    
    private final OsType osType;
    private Charset outputCharset;
    private File workingDirectory;
    private Map<String, String> environment;
    private long timeoutSeconds = 300;
    private List<String> commonPrefixCommands;
    
    public StringCommandExecutor() {
        this.osType = detectOsType();
        this.environment = new HashMap<>();
        this.commonPrefixCommands = new ArrayList<>();
        
        // 根据操作系统设置默认编码
        if (osType == OsType.WINDOWS) {
            this.outputCharset = Charset.forName("GBK");
        } else {
            this.outputCharset = StandardCharsets.UTF_8;
        }
    }
    
    private OsType detectOsType() {
        String os = System.getProperty("os.name").toLowerCase();
        if (os.contains("win")) {
            return OsType.WINDOWS;
        }
        return OsType.UNIX;
    }
    
    public StringCommandExecutor withOutputEncoding(String charsetName) {
        this.outputCharset = Charset.forName(charsetName);
        return this;
    }
    
    public StringCommandExecutor workingDirectory(String path) {
        this.workingDirectory = new File(path);
        return this;
    }
    
    public StringCommandExecutor timeout(long seconds) {
        this.timeoutSeconds = seconds;
        return this;
    }
    
    /**
     * 添加公共前置命令（字符串形式）
     */
    public StringCommandExecutor addPrefixCommand(String command) {
        this.commonPrefixCommands.add(command);
        return this;
    }
    
    /**
     * 执行单个命令 - 直接传入字符串
     */
    public CommandResult execute(String commandString) throws CommandExecutionException {
        // 构建完整命令（包含前置命令）
        String fullCommand = buildFullCommand(commandString);
        
        try {
            System.out.println("【执行命令】" + fullCommand);
            
            ProcessBuilder pb;
            if (osType == OsType.WINDOWS) {
                // Windows: cmd.exe /c "命令"
                pb = new ProcessBuilder("cmd.exe", "/c", fullCommand);
            } else {
                // Linux/Mac: sh -c "命令"
                pb = new ProcessBuilder("sh", "-c", fullCommand);
            }
            
            if (workingDirectory != null) {
                pb.directory(workingDirectory);
                System.out.println("【工作目录】" + workingDirectory.getAbsolutePath());
            }
            
            if (!environment.isEmpty()) {
                pb.environment().putAll(environment);
            }
            
            // 分别读取标准输出和错误输出
            pb.redirectErrorStream(false);
            
            Process process = pb.start();
            
            StringBuilder output = new StringBuilder();
            StringBuilder error = new StringBuilder();
            
            // 读取标准输出
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
            
            // 读取错误输出
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
                throw new CommandExecutionException("命令执行超时");
            }
            
            // 等待输出读取完成
            outputFuture.get(5, TimeUnit.SECONDS);
            errorFuture.get(5, TimeUnit.SECONDS);
            
            int exitCode = process.exitValue();
            
            String outputStr = output.toString();
            String errorStr = error.toString();
            
            System.out.println("【命令完成】退出码=" + exitCode + ", 输出长度=" + outputStr.length() + 
                             ", 错误长度=" + errorStr.length());
            
            return new CommandResult(exitCode, outputStr, errorStr, exitCode == 0);
            
        } catch (IOException | InterruptedException | ExecutionException | TimeoutException e) {
            throw new CommandExecutionException("命令执行失败: " + e.getMessage(), e);
        }
    }
    
    /**
     * 执行命令链 - 使用字符串回调
     */
    public List<CommandResult> executeChain(List<StringCommandCallback> callbacks) 
            throws CommandExecutionException {
        List<CommandResult> results = new ArrayList<>();
        CommandResult previousResult = null;
        
        for (int i = 0; i < callbacks.size(); i++) {
            System.out.println("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
            System.out.println("【Callback " + (i + 1) + "】");
            System.out.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
            
            StringCommandCallback callback = callbacks.get(i);
            
            // 打印上一个结果的信息
            if (previousResult != null) {
                System.out.println("【上一个命令结果】");
                System.out.println("  成功: " + previousResult.isSuccess());
                System.out.println("  退出码: " + previousResult.getExitCode());
                System.out.println("  输出长度: " + previousResult.getOutput().length() + " 字符");
                System.out.println("  错误长度: " + previousResult.getError().length() + " 字符");
                
                if (!previousResult.getOutput().isEmpty()) {
                    // 只显示前200字符的预览
                    String preview = previousResult.getOutput();
                    if (preview.length() > 200) {
                        preview = preview.substring(0, 200) + "...";
                    }
                    System.out.println("  输出预览: " + preview);
                }
            } else {
                System.out.println("【上一个命令结果】null (这是第一个命令)");
            }
            
            // 调用 callback 构造命令（返回字符串）
            String commandString = callback.buildCommand(previousResult);
            
            if (commandString == null || commandString.isEmpty()) {
                System.out.println("【结束】callback 返回 null/空字符串,命令链结束");
                break;
            }
            
            // 执行命令
            CommandResult result = execute(commandString);
            results.add(result);
            
            // 检查是否需要停止
            if (!result.isSuccess() && callback.stopOnFailure()) {
                System.out.println("【停止】命令失败且 stopOnFailure=true");
                break;
            }
            
            previousResult = result;
        }
        
        System.out.println("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        System.out.println("【命令链完成】共执行 " + results.size() + " 个命令");
        System.out.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
        
        return results;
    }
    
    /**
     * 构建完整命令（包含前置命令）
     */
    private String buildFullCommand(String command) {
        if (commonPrefixCommands.isEmpty()) {
            return command;
        }
        
        // 用 && 连接所有命令
        List<String> allCommands = new ArrayList<>(commonPrefixCommands);
        allCommands.add(command);
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
            return "CommandResult{exitCode=" + exitCode + ", success=" + success + 
                   ", outputLength=" + output.length() + ", errorLength=" + error.length() + "}";
        }
    }
    
    /**
     * 字符串命令回调接口
     * 返回字符串命令，而不是字符串数组
     */
    @FunctionalInterface
    public interface StringCommandCallback {
        /**
         * 根据上一个命令的结果构造下一个命令（字符串形式）
         * @param previousResult 上一个命令的结果
         * @return 要执行的命令字符串，返回 null 表示结束链
         */
        String buildCommand(CommandResult previousResult);
        
        /**
         * 是否在失败时停止
         */
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
