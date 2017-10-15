function u_zz = Dzz(u,h,order,bcType)

if nargin < 4
  bcType = 'not periodic';
end

% if order > 2 && strcmp(bcType,'periodic')
%   fprintf('Error: periodic boundary conditions not supported for order > 2\n')
%   return
% end

u_zz = 0*u;
[m,n] = size(u);

if(order == 2)
  if strcmp(bcType,'periodic')
    u_zz(1,:) = u(end-1, :) - 2*u(1,:) + u(2,:);
    u_zz(end,:) =u(end-1,:) - 2*u(end,:) + u(2,:);
  end
  for i = 2:m-1
    u_zz(i,:) = u(i+1,:) - 2*u(i,:) + u(i-1,:);
  end
elseif(order == 4)
  %         u_yy(1,:) = 1.319753349375470*u(1,:) - 2.279013397501878*u(2,:) - 0.081479903747182*u(3,:) + 1.720986602498121*u(4,:)...
  %             - 0.680246650624530*u(5,:);
  %
  %         u_yy(2,:) = 0.901112191978006*u(1,:) - 1.593787843034701*u(2,:) + 0.364029452358750*u(3,:) + 0.459516781351903*u(4,:)...
  %             - 0.141531507531278*u(5,:) + 0.010660924877321*u(6,:);
  %
  %         u_yy(3,:) = 0.023650158582586*u(1,:) + 0.796439026381821*u(2,:) - 1.422257691353143*u(3,:) + 0.251637329942644*u(4,:)...
  %             + 0.459491515733928*u(5,:) - 0.108960339287835*u(6,:);
  %
  %         u_yy(4,:) =-0.120536120137894*u(1,:) + 0.518872114613017*u(2,:) + 0.129872742926870*u(3,:) - 1.297489715079774*u(4,:)...
  %             + 0.732553343616339*u(5,:) + 0.036727634061442*u(6,:);
  %
  %         u_yy(5,:) = 0.044476945566742*u(1,:) - 0.212643239032514*u(2,:) + 0.315544004411490*u(3,:) + 0.974717461344343*u(4,:)...
  %             - 2.358138203677955*u(5,:) + 1.326302527439042*u(6,:) - 0.090259496051147*u(7,:);
  %
  %         u_yy(6,:) = 0.014642919234304*u(2,:) - 0.068404586240661*u(3,:) + 0.044675228394891*u(4,:) + 1.212486564140966*u(5,:)...
  %             - 2.441108988900194*u(6,:) + 1.320222787595407*u(7,:) - 0.082513924224713*u(8,:);
  %
  %
  %         u_yy(m-5,:) =-0.082513924224713*u(m-7,:) + 1.320222787595407*u(m-6,:) - 2.441108988900194*u(m-5,:) + 1.212486564140966*u(m-4,:)...
  %             + 0.044675228394891*u(m-3,:) - 0.068404586240661*u(m-2,:) + 0.014642919234304*u(m-1,:);
  %
  %         u_yy(m-4,:) =-0.090259496051147*u(m-6,:) + 1.326302527439042*u(m-5,:) - 2.358138203677955*u(m-4,:) + 0.974717461344343*u(m-3,:)...
  %             + 0.315544004411490*u(m-2,:) - 0.212643239032514*u(m-1,:) + 0.044476945566742*u(m,:);
  %
  %         u_yy(m-3,:) = 0.036727634061442*u(m-5,:) + 0.732553343616339*u(m-4,:) - 1.297489715079774*u(m-3,:) + 0.129872742926870*u(m-2,:)...
  %             + 0.518872114613017*u(m-1,:) - 0.120536120137894*u(m,:);
  %
  %         u_yy(m-2,:) =-0.108960339287835*u(m-5,:) + 0.459491515733928*u(m-4,:) + 0.251637329942644*u(m-3,:) - 1.422257691353143*u(m-2,:)...
  %             + 0.796439026381821*u(m-1,:) + 0.023650158582586*u(m,:);
  %
  %         u_yy(m-1,:) = 0.010660924877321*u(m-5,:) - 0.141531507531278*u(m-4,:) + 0.459516781351903*u(m-3,:) + 0.364029452358750*u(m-2,:)...
  %             - 1.593787843034701*u(m-1,:) + 0.901112191978006*u(m,:);
  %
  %         u_yy(m,:)   =-0.680246650624530*u(m-4,:) + 1.720986602498121*u(m-3,:) - 0.081479903747182*u(m-2,:) - 2.279013397501878*u(m-1,:)...
  %             + 1.319753349375470*u(m,:);
  %?1/12 	4/3 	?5/2 	4/3 	?1/12
  if strcmp(bcType,'periodic')
    u_zz(1,:) =-1/12*u(m-2,:) + 4/3*u(m-1,:) - 2.5*u(1,:) + 4/3*u(2,:)...
      - 1/12*u(3,:);
    u_zz(2,:) =-1/12*u(m-1,:) + 4/3*u(1,:) - 2.5*u(2,:) + 4/3*u(3,:)...
      - 1/12*u(4,:);
    u_zz(m-1,:) =-1/12*u(m-3,:) + 4/3*u(m-2,:) - 2.5*u(m-1,:) + 4/3*u(m,:)...
      - 1/12*u(2,:);
    u_zz(m,:) =-1/12*u(m-2,:) + 4/3*u(m-1,:) - 2.5*u(m,:) + 4/3*u(2,:)...
      - 1/12*u(3,:);
    
    for i = 3:m-2
      u_zz(i,:) =-1/12*u(i-2,:) + 4/3*u(i-1,:) - 5/2*u(i,:) + 4/3*u(i+1,:) - 1/12*u(i+2,:);
    end
  else
    u_zz(1,:) = 1.319753349375470*u(1,:) - 2.279013397501878*u(2,:) - 0.081479903747182*u(3,:) + 1.720986602498121*u(4,:)...
      - 0.680246650624530*u(5,:);
    
    u_zz(2,:) = 0.901112191978006*u(1,:) - 1.593787843034701*u(2,:) + 0.364029452358750*u(3,:) + 0.459516781351903*u(4,:)...
      - 0.141531507531278*u(5,:) + 0.010660924877321*u(6,:);
    
    u_zz(3,:) = 0.023650158582586*u(1,:) + 0.796439026381821*u(2,:) - 1.422257691353143*u(3,:) + 0.251637329942644*u(4,:)...
      + 0.459491515733928*u(5,:) - 0.108960339287835*u(6,:);
    
    u_zz(4,:) =-0.120536120137894*u(1,:) + 0.518872114613017*u(2,:) + 0.129872742926870*u(3,:) - 1.297489715079774*u(4,:)...
      + 0.732553343616339*u(5,:) + 0.036727634061442*u(6,:);
    
    u_zz(5,:) = 0.044476945566742*u(1,:) - 0.212643239032514*u(2,:) + 0.315544004411490*u(3,:) + 0.974717461344343*u(4,:)...
      - 2.358138203677955*u(5,:) + 1.326302527439042*u(6,:) - 0.090259496051147*u(7,:);
    
    u_zz(6,:) = 0.014642919234304*u(2,:) - 0.068404586240661*u(3,:) + 0.044675228394891*u(4,:) + 1.212486564140966*u(5,:)...
      - 2.441108988900194*u(6,:) + 1.320222787595407*u(7,:) - 0.082513924224713*u(8,:);
    
    
    u_zz(m-5,:) =-0.082513924224713*u(m-7,:) + 1.320222787595407*u(m-6,:) - 2.441108988900194*u(m-5,:) + 1.212486564140966*u(m-4,:)...
      + 0.044675228394891*u(m-3,:) - 0.068404586240661*u(m-2,:) + 0.014642919234304*u(m-1,:);
    
    u_zz(m-4,:) =-0.090259496051147*u(m-6,:) + 1.326302527439042*u(m-5,:) - 2.358138203677955*u(m-4,:) + 0.974717461344343*u(m-3,:)...
      + 0.315544004411490*u(m-2,:) - 0.212643239032514*u(m-1,:) + 0.044476945566742*u(m,:);
    
    u_zz(m-3,:) = 0.036727634061442*u(m-5,:) + 0.732553343616339*u(m-4,:) - 1.297489715079774*u(m-3,:) + 0.129872742926870*u(m-2,:)...
      + 0.518872114613017*u(m-1,:) - 0.120536120137894*u(m,:);
    
    u_zz(m-2,:) =-0.108960339287835*u(m-5,:) + 0.459491515733928*u(m-4,:) + 0.251637329942644*u(m-3,:) - 1.422257691353143*u(m-2,:)...
      + 0.796439026381821*u(m-1,:) + 0.023650158582586*u(m,:);
    
    u_zz(m-1,:) = 0.010660924877321*u(m-5,:) - 0.141531507531278*u(m-4,:) + 0.459516781351903*u(m-3,:) + 0.364029452358750*u(m-2,:)...
      - 1.593787843034701*u(m-1,:) + 0.901112191978006*u(m,:);
    
    u_zz(m,:)   =-0.680246650624530*u(m-4,:) + 1.720986602498121*u(m-3,:) - 0.081479903747182*u(m-2,:) - 2.279013397501878*u(m-1,:)...
      + 1.319753349375470*u(m,:);
    for i = 7:m-6
      u_zz(i,:) =-0.083333333333333*u(i-2,:) + 1.333333333333333*u(i-1,:) - 2.5*u(i,:) + 1.333333333333333*u(i+1,:)...
        - 0.083333333333333*u(i+2,:);
    end
  end
  
elseif(order == 6)
  u_zz(1,:) = 1.677132509479325*u(1,:) - 3.038214359671524*u(2,:) - 0.049249177165591*u(3,:) + 2.036490115791102*u(4,:)...
    + 0.411180670145605*u(5,:) - 1.606558237520481*u(6,:) + 0.569218478941565*u(7,:);
  
  u_zz(2,:) = 0.884778483741509*u(1,:) - 1.558734408191758*u(2,:) + 0.443635669435212*u(3,:) + 0.112257511414158*u(4,:)...
    + 0.322054492873589*u(5,:) - 0.275397277153155*u(6,:) + 0.076379745812837*u(7,:) - 0.004974217932391*u(8,:);
  
  u_zz(3,:) =-0.270618371836400*u(1,:) + 2.608899664176471*u(2,:) - 6.229396164399805*u(3,:) + 7.401795540292633*u(4,:)...
    - 6.019570786427897*u(5,:) + 3.491391607443278*u(6,:) - 1.143341576978401*u(7,:) + 0.160840087730119*u(8,:);
  
  u_zz(4,:) =-0.038775768114076*u(1,:) + 0.095390654740871*u(2,:) + 1.069538898422949*u(3,:) - 2.504735762568496*u(4,:)...
    + 1.772892855219715*u(5,:) - 0.602221665195144*u(6,:) + 0.256099771243681*u(7,:) - 0.048188983749501*u(8,:);
  
  u_zz(5,:) =-0.161791607686480*u(1,:) + 1.177738635455464*u(2,:) - 3.743295966085387*u(3,:) + 7.629773234288446*u(4,:)...
    - 8.978199716382386*u(5,:) + 5.320463633854483*u(6,:) - 1.442831784216072*u(7,:) + 0.198143570771935*u(8,:);
  
  u_zz(6,:) = 0.061315880970547*u(1,:) - 0.329403900039681*u(2,:) + 0.710128166312073*u(3,:) - 0.847686078265718*u(4,:)...
    + 1.740199396764717*u(5,:) - 2.591052963604934*u(6,:) + 1.357538031860576*u(7,:) - 0.109753113995674*u(8,:)...
    + 0.008714579998094*u(9,:);
  
  u_zz(7,:) =-0.021840052913065*u(1,:) + 0.125943642664851*u(2,:) - 0.320584941290064*u(3,:) + 0.496954711803904*u(4,:)...
    - 0.650570224322142*u(5,:) + 1.871461736877637*u(6,:) - 2.917045539749188*u(7,:) + 1.565851441490449*u(8,:)...
    - 0.162184436527372*u(9,:) + 0.012013661964991*u(10,:);
  
  u_zz(8,:) =-0.007517959778232*u(2,:) + 0.041336963439990*u(3,:) - 0.085710205677551*u(4,:) + 0.081890894364834*u(5,:)...
    - 0.138682909879055*u(6,:) + 1.435250490561686*u(7,:) - 2.675494883306515*u(8,:) + 1.486573284792685*u(9,:)...
    - 0.148657328479269*u(10,:) + 0.011011653961427*u(11,:);
  
  
  u_zz(m-7,:) = 0.011011653961427*u(m-10,:) - 0.148657328479269*u(m-9,:) + 1.486573284792685*u(m-8,:) - 2.675494883306515*u(m-7,:)...
    + 1.435250490561686*u(m-6,:) - 0.138682909879055*u(m-5,:) + 0.081890894364834*u(m-4,:) - 0.085710205677551*u(m-3,:)...
    + 0.041336963439990*u(m-2,:) - 0.007517959778232*u(m-1,:);
  
  u_zz(m-6,:) = 0.012013661964991*u(m-9,:)  - 0.162184436527372*u(m-8,:) + 1.565851441490449*u(m-7,:) - 2.917045539749188*u(m-6,:)...
    + 1.871461736877637*u(m-5,:) - 0.650570224322142*u(m-4,:) + 0.496954711803904*u(m-3,:) - 0.320584941290064*u(m-2,:)...
    + 0.125943642664851*u(m-1,:) - 0.021840052913065*u(m,:);
  
  u_zz(m-5,:) = 0.008714579998094*u(m-8,:)  - 0.109753113995674*u(m-7,:) + 1.357538031860576*u(m-6,:) - 2.591052963604934*u(m-5,:)...
    + 1.740199396764717*u(m-4,:) - 0.847686078265718*u(m-3,:) + 0.710128166312073*u(m-2,:) - 0.329403900039681*u(m-1,:)...
    + 0.061315880970547*u(m,:);
  
  u_zz(m-4,:) = 0.198143570771935*u(m-7,:)  - 1.442831784216072*u(m-6,:) + 5.320463633854483*u(m-5,:) - 8.978199716382386*u(m-4,:)...
    + 7.629773234288446*u(m-3,:) - 3.743295966085387*u(m-2,:) + 1.177738635455464*u(m-1,:) - 0.161791607686480*u(m,:);
  
  u_zz(m-3,:) =-0.048188983749501*u(m-7,:)  + 0.256099771243681*u(m-6,:) - 0.602221665195144*u(m-5,:) + 1.772892855219715*u(m-4,:)...
    - 2.504735762568496*u(m-3,:) + 1.069538898422949*u(m-2,:) + 0.095390654740871*u(m-1,:) - 0.038775768114076*u(m,:);
  
  u_zz(m-2,:) = 0.160840087730119*u(m-7,:)  - 1.143341576978401*u(m-6,:) + 3.491391607443278*u(m-5,:) - 6.019570786427897*u(m-4,:)...
    + 7.401795540292633*u(m-3,:) - 6.229396164399805*u(m-2,:) + 2.608899664176471*u(m-1,:) - 0.270618371836400*u(m,:);
  
  u_zz(m-1,:) =-0.004974217932391*u(m-7,:)  + 0.076379745812837*u(m-6,:) - 0.275397277153155*u(m-5,:) + 0.322054492873589*u(m-4,:)...
    + 0.112257511414158*u(m-3,:) + 0.443635669435212*u(m-2,:) - 1.558734408191758*u(m-1,:) + 0.884778483741509*u(m,:);
  
  u_zz(m,:)   = 0.569218478941565*u(m-6,:)  - 1.606558237520481*u(m-5,:) + 0.411180670145605*u(m-4,:) + 2.036490115791102*u(m-3,:)...
    - 0.049249177165591*u(m-2,:) - 3.038214359671524*u(m-1,:) + 1.677132509479325*u(m,:);
  
  for i = 9:m-8
    u_zz(i,:) = 0.011111111111111*u(i-3,:) - 0.15*u(i-2,:) + 1.5*u(i-1,:) - 2.722222222222222*u(i,:) + 1.5*u(i+1,:) - 0.15*u(i+2,:)...
      + 0.011111111111111*u(i+3,:);
  end
end

u_zz = (1/h^2)*u_zz;
end