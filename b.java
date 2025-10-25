import java.util.*;

/**
 * CommandExecutor 使用示例
 */
public class CommandExecutorExample {
    
    public static void main(String[] args) {
        // 示例1: 基本使用
        basicUsage();
        
        // 示例2: 使用公共前置命令
        withPrefixCommands();
        
        // 示例3: 命令链执行 - 使用回调列表
        commandChainWithCallbacks();
        
        // 示例4: 命令链执行 - 使用构造器
        commandChainWithBuilder();
        
        // 示例5: Docker 环境执行
        dockerExecution();
        
        // 示例6: 复杂的命令链示例
        complexCommandChain();
    }
    
    /**
     * 示例1: 基本使用
     */
    public static void basicUsage() {
        System.out.println("=== 示例1: 基本使用 ===");
        try {
            CommandExecutor executor = new CommandExecutor();
            
            // 执行简单命令
            CommandExecutor.CommandResult result = executor.execute("echo", "Hello World");
            
            System.out.println("执行结果: " + result.isSuccess());
            System.out.println("输出: " + result.getOutput());
            System.out.println();
            
        } catch (CommandExecutor.CommandExecutionException e) {
            e.printStackTrace();
        }
    }
    
    /**
     * 示例2: 使用公共前置命令
     */
    public static void withPrefixCommands() {
        System.out.println("=== 示例2: 使用公共前置命令 ===");
        try {
            CommandExecutor executor = new CommandExecutor()
                    .withPrefixCommands("cd /tmp", "export PATH=$PATH:/usr/local/bin")
                    .timeout(60);
            
            // 所有命令都会在 /tmp 目录下执行，并且有自定义的 PATH
            CommandExecutor.CommandResult result = executor.execute("pwd");
            
            System.out.println("当前目录: " + result.getOutput());
            System.out.println();
            
        } catch (CommandExecutor.CommandExecutionException e) {
            e.printStackTrace();
        }
    }
    
    /**
     * 示例3: 命令链执行 - 使用回调列表
     */
    public static void commandChainWithCallbacks() {
        System.out.println("=== 示例3: 命令链执行 - 使用回调 ===");
        try {
            CommandExecutor executor = new CommandExecutor();
            
            List<CommandExecutor.CommandCallback> callbacks = new ArrayList<>();
            
            // 第一个命令: 创建临时目录
            callbacks.add(previousResult -> 
                new String[]{"mktemp", "-d"}
            );
            
            // 第二个命令: 在临时目录中创建文件
            callbacks.add(previousResult -> {
                if (previousResult != null && previousResult.isSuccess()) {
                    String tmpDir = previousResult.getOutput().trim();
                    return new String[]{"touch", tmpDir + "/test.txt"};
                }
                return null; // 结束链
            });
            
            // 第三个命令: 列出目录内容
            callbacks.add(previousResult -> {
                if (previousResult != null && previousResult.isSuccess()) {
                    // 从第一个结果中获取目录路径（需要存储）
                    // 这里简化处理
                    return new String[]{"echo", "文件已创建"};
                }
                return null;
            });
            
            List<CommandExecutor.CommandResult> results = executor.executeChain(callbacks);
            
            System.out.println("执行了 " + results.size() + " 个命令");
            for (int i = 0; i < results.size(); i++) {
                System.out.println("命令 " + (i + 1) + " 输出: " + results.get(i).getOutput());
            }
            System.out.println();
            
        } catch (CommandExecutor.CommandExecutionException e) {
            e.printStackTrace();
        }
    }
    
    /**
     * 示例4: 命令链执行 - 使用构造器
     */
    public static void commandChainWithBuilder() {
        System.out.println("=== 示例4: 命令链执行 - 使用构造器 ===");
        try {
            CommandExecutor executor = new CommandExecutor();
            
            // 使用构造器模式创建命令链
            CommandExecutor.CommandChainBuilder builder = 
                new CommandExecutor.CommandChainBuilder()
                    .firstCommand("echo step1")
                    .then(result -> {
                        if (result.isSuccess()) {
                            return "echo step2 after " + result.getOutput();
                        }
                        return null;
                    });
            
            CommandExecutor.CommandResult result = executor.executeChain(builder);
            System.out.println("最终结果: " + result.getOutput());
            System.out.println();
            
        } catch (CommandExecutor.CommandExecutionException e) {
            e.printStackTrace();
        }
    }
    
    /**
     * 示例5: Docker 环境执行
     */
    public static void dockerExecution() {
        System.out.println("=== 示例5: Docker 环境执行 ===");
        try {
            CommandExecutor executor = new CommandExecutor()
                    .inDocker("my-container")
                    .withPrefixCommands("cd /app")
                    .timeout(120);
            
            // 在 Docker 容器中执行命令
            CommandExecutor.CommandResult result = executor.execute("ls", "-la");
            
            System.out.println("Docker 容器中的文件列表:");
            System.out.println(result.getOutput());
            System.out.println();
            
        } catch (CommandExecutor.CommandExecutionException e) {
            System.out.println("注意: 需要有运行中的 Docker 容器");
            System.out.println();
        }
    }
    
    /**
     * 示例6: 复杂的命令链示例 - 查找文件并处理
     */
    public static void complexCommandChain() {
        System.out.println("=== 示例6: 复杂的命令链示例 ===");
        try {
            CommandExecutor executor = new CommandExecutor()
                    .workingDirectory(System.getProperty("user.home"))
                    .timeout(30);
            
            List<CommandExecutor.CommandCallback> callbacks = new ArrayList<>();
            
            // 步骤1: 查找所有 .txt 文件
            callbacks.add(previousResult -> 
                new String[]{"find", ".", "-name", "*.txt", "-type", "f", "-maxdepth", "2"}
            );
            
            // 步骤2: 统计找到的文件数量
            callbacks.add(previousResult -> {
                if (previousResult != null && previousResult.isSuccess()) {
                    String output = previousResult.getOutput();
                    String[] files = output.split("\n");
                    System.out.println("找到 " + files.length + " 个 .txt 文件");
                    
                    if (files.length > 0) {
                        // 返回查看第一个文件的命令
                        return new String[]{"head", "-n", "5", files[0].trim()};
                    }
                }
                return null;
            });
            
            // 步骤3: 根据文件内容做进一步处理
            callbacks.add(new CommandExecutor.CommandCallback() {
                @Override
                public String[] buildCommand(CommandExecutor.CommandResult previousResult) {
                    if (previousResult != null && previousResult.isSuccess()) {
                        // 可以根据上一步的输出决定下一步操作
                        return new String[]{"echo", "文件内容已读取"};
                    }
                    return null;
                }
                
                @Override
                public boolean stopOnFailure() {
                    return false; // 即使失败也继续
                }
            });
            
            List<CommandExecutor.CommandResult> results = executor.executeChain(callbacks);
            
            System.out.println("\n命令链执行完成:");
            for (int i = 0; i < results.size(); i++) {
                CommandExecutor.CommandResult result = results.get(i);
                System.out.println("步骤 " + (i + 1) + ": " + 
                    (result.isSuccess() ? "成功" : "失败"));
            }
            System.out.println();
            
        } catch (CommandExecutor.CommandExecutionException e) {
            e.printStackTrace();
        }
    }
}

/**
 * 实际应用场景示例
 */
class RealWorldExamples {
    
    /**
     * 场景1: Git 操作流程
     */
    public static void gitWorkflow() throws CommandExecutor.CommandExecutionException {
        CommandExecutor executor = new CommandExecutor()
                .workingDirectory("/path/to/repo")
                .withPrefixCommands("git config user.name 'Bot'");
        
        List<CommandExecutor.CommandCallback> callbacks = new ArrayList<>();
        
        // 1. 检查当前分支
        callbacks.add(prev -> new String[]{"git", "branch", "--show-current"});
        
        // 2. 拉取最新代码
        callbacks.add(prev -> new String[]{"git", "pull", "origin", prev.getOutput().trim()});
        
        // 3. 创建新分支
        callbacks.add(prev -> {
            String branchName = "feature-" + System.currentTimeMillis();
            return new String[]{"git", "checkout", "-b", branchName};
        });
        
        // 4. 添加文件
        callbacks.add(prev -> new String[]{"git", "add", "."});
        
        // 5. 提交
        callbacks.add(prev -> new String[]{"git", "commit", "-m", "Auto commit"});
        
        executor.executeChain(callbacks);
    }
    
    /**
     * 场景2: 构建和部署流程
     */
    public static void buildAndDeploy() throws CommandExecutor.CommandExecutionException {
        CommandExecutor executor = new CommandExecutor()
                .workingDirectory("/path/to/project")
                .withEnvironment("NODE_ENV", "production")
                .timeout(600); // 10分钟超时
        
        List<CommandExecutor.CommandCallback> callbacks = new ArrayList<>();
        
        // 1. 安装依赖
        callbacks.add(prev -> new String[]{"npm", "install"});
        
        // 2. 运行测试
        callbacks.add(prev -> new String[]{"npm", "test"});
        
        // 3. 构建项目
        callbacks.add(prev -> {
            if (prev.isSuccess()) {
                return new String[]{"npm", "run", "build"};
            }
            return null; // 测试失败则不构建
        });
        
        // 4. 部署
        callbacks.add(prev -> {
            if (prev.isSuccess()) {
                return new String[]{"npm", "run", "deploy"};
            }
            return null;
        });
        
        executor.executeChain(callbacks);
    }
    
    /**
     * 场景3: Docker 容器管理
     */
    public static void dockerManagement() throws CommandExecutor.CommandExecutionException {
        CommandExecutor executor = new CommandExecutor();
        
        List<CommandExecutor.CommandCallback> callbacks = new ArrayList<>();
        
        // 1. 检查容器是否存在
        callbacks.add(prev -> new String[]{"docker", "ps", "-a", "--filter", "name=myapp", "--format", "{{.Names}}"});
        
        // 2. 停止旧容器
        callbacks.add(prev -> {
            if (prev.isSuccess() && !prev.getOutput().isEmpty()) {
                return new String[]{"docker", "stop", "myapp"};
            }
            return new String[]{"echo", "无需停止容器"};
        });
        
        // 3. 删除旧容器
        callbacks.add(prev -> new String[]{"docker", "rm", "myapp"});
        
        // 4. 拉取新镜像
        callbacks.add(prev -> new String[]{"docker", "pull", "myapp:latest"});
        
        // 5. 启动新容器
        callbacks.add(prev -> new String[]{"docker", "run", "-d", "--name", "myapp", "myapp:latest"});
        
        executor.executeChain(callbacks);
    }
    
    /**
     * 场景4: 数据库备份流程
     */
    public static void databaseBackup() throws CommandExecutor.CommandExecutionException {
        CommandExecutor executor = new CommandExecutor()
                .inDocker("mysql-container");
        
        List<CommandExecutor.CommandCallback> callbacks = new ArrayList<>();
        
        // 1. 创建备份目录
        String backupDate = java.time.LocalDate.now().toString();
        callbacks.add(prev -> new String[]{"mkdir", "-p", "/backups/" + backupDate});
        
        // 2. 执行数据库备份
        callbacks.add(prev -> new String[]{
            "mysqldump", 
            "-u", "root", 
            "-p$MYSQL_ROOT_PASSWORD",
            "--all-databases",
            ">", "/backups/" + backupDate + "/backup.sql"
        });
        
        // 3. 压缩备份文件
        callbacks.add(prev -> {
            if (prev.isSuccess()) {
                return new String[]{
                    "gzip", 
                    "/backups/" + backupDate + "/backup.sql"
                };
            }
            return null;
        });
        
        // 4. 验证备份文件
        callbacks.add(prev -> new String[]{"ls", "-lh", "/backups/" + backupDate + "/"});
        
        executor.executeChain(callbacks);
    }
}
