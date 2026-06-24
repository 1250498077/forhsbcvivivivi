@echo off
chcp 65001 >nul

set /p msg=请输入 commit 信息: 

if "%msg%"=="" (
    echo 错误：commit 信息不能为空！
    pause
    exit /b 1
)

echo.
echo [1/3] git add .
git add .

echo [2/3] git commit -m "%msg%"
git commit -m "%msg%"

echo [3/3] git push
git push

echo.
echo 完成！
pause